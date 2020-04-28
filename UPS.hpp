#ifndef UPS_HPP__
#define UPS_HPP__

#include "UA.pb.h"
#include "world_ups.pb.h"
#include "logger.hpp"
#include "socket.hpp"
#include "package.hpp"
#include "constants.hpp"
#include "truckpool.hpp"
#include "dataGenerator.hpp"
#include "databaseLogger.hpp"
#include "sequenceGenerator.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <exception>
#define SLEEP_PERIOD 30
#define MAX_ERR_COUNT 20

class UPS
{
private:
    void queryTruck(int truckid);
    void handleTruckStatusQuery(const UTruck truckStatusQueryRes);
    void sendAckMessageToAmazon(unsigned seqNum);
    void sendAckMessageToWorld(unsigned seqNum);
    void handleAmazonErrMsg(const ErrorMessage errMsg);
    void handleWorldErrMsg(const UErr errMsg);
    void handleAck(int ack);

    // receive user validation request from Amazon
    void handleUserValidationReq(const UserValidationRequest fromAmazonUserValidationReq);

    // receive from Amazon to pick up, ask world to pick up
    void handlePickupReq(const AtoUPickupRequest fromAmazonPickUpReq);

    // receive from Amazon load has completed, ask world to deliver
    void handleDeliveryReq(const AtoULoadFinishRequest fromAmazonDeliverReq);

    // there're two possiblities for receiving UFinished
    // (1) world notify the truck arrive the warehouse, ask Amazon to load
    // (2) world notify the truck finished all shipments, return truck to truck pool
    void handleLoadReq(const UFinished fromWorldToLoadReq);

    // world delivery made, notify Amazon success
    // UDeliveryMade is different with UFinished:
    // UDeliveryMade is used when any one delivery is made
    // UFinished is sent when all the deliveries are made for a truck
    void handleDeliveryMadeRes(const UDeliveryMade fromWorldDeliveryMade);

    void handleAmazonRes()
    {
        while(true)
        {
            AtoUCommand amazonCommand;
            bool recvSuc = amazonSocket->recvMsg(amazonCommand);
            if(!recvSuc)
            {
                Logger::getInstance()->log("error.log", "Receive from Amazon error");
                if(++errCount >= MAX_ERR_COUNT)
                {
                    Logger::getInstance()->log("error.log", "Reach max error count, program exits");
                    exit(EXIT_FAILURE);
                }
                continue;
            }

            int userValidationReqCnt = amazonCommand.usrvlid_size();
            int pickupReqCnt = amazonCommand.pikreq_size();
            int loadReqCnt = amazonCommand.loadreq_size();
            int errMsgCnt = amazonCommand.errmsg_size();
            int ackCnt = amazonCommand.ack_size();

            for(int idx = 0; idx < userValidationReqCnt; ++idx)
            {
                const UserValidationRequest & userValidationReq = amazonCommand.usrvlid(idx);
                std::thread thd(&UPS::handleUserValidationReq, this, userValidationReq);
                thd.detach();
            }

            for(int idx = 0; idx < pickupReqCnt; ++idx)
            {
                const AtoUPickupRequest & pickupReq = amazonCommand.pikreq(idx);
                std::thread thd(&UPS::handlePickupReq, this, pickupReq);
                thd.detach();
            }

            for(int idx = 0; idx < loadReqCnt; ++idx)
            {
                const AtoULoadFinishRequest & deliveryReq = amazonCommand.loadreq(idx);
                std::thread thd(&UPS::handleDeliveryReq, this, deliveryReq);
                thd.detach();
            }

            for(int idx = 0; idx < errMsgCnt; ++idx)
            {
                const ErrorMessage & errMsg = amazonCommand.errmsg(idx);
                std::thread thd(&UPS::handleAmazonErrMsg, this, errMsg);
                thd.detach();
            }

            for(int idx = 0; idx < ackCnt; ++idx)
            {
                int ack = amazonCommand.ack(idx);
                std::thread thd(&UPS::handleAck, this, ack);
                thd.detach();
            }
        }
    }

    void handleWorldRes()
    {
        while(true)
        {
            UResponses worldRes;
            bool recvSuc = worldSocket->recvMsg(worldRes);
            if(!recvSuc)
            {
                Logger::getInstance()->log("error.log", "Receive from world error");
                if(++errCount >= MAX_ERR_COUNT)
                {
                    Logger::getInstance()->log("error.log", "Reach max error count, program exits");
                    exit(EXIT_FAILURE);
                }
                continue;
            }

            int pickupResCnt = worldRes.completions_size();
            int deliveryMadeCnt = worldRes.delivered_size();
            int ackCnt = worldRes.acks_size();
            int errMsgCnt = worldRes.error_size();
            int truckStatusCnt = worldRes.truckstatus_size();

            for(int idx = 0; idx < pickupResCnt; ++idx)
            {
                const UFinished & loadReq = worldRes.completions(idx);
                std::thread thd(&UPS::handleLoadReq, this, loadReq);
                thd.detach();
            }

            for(int idx = 0; idx < deliveryMadeCnt; ++idx)
            {
                const UDeliveryMade & deliveryMade = worldRes.delivered(idx);
                std::thread thd(&UPS::handleDeliveryMadeRes, this, deliveryMade);
                thd.detach(); 
            }

            for(int idx = 0; idx < ackCnt; ++idx)
            {
                int ack = worldRes.acks(idx);
                std::thread thd(&UPS::handleAck, this, ack);
                thd.detach();
            }

            for(int idx = 0; idx < errMsgCnt; ++idx)
            {
                const UErr & errMsg = worldRes.error(idx);
                std::thread thd(&UPS::handleWorldErrMsg, this, errMsg);
                thd.detach(); 
            }

            for(int idx = 0; idx < truckStatusCnt; ++idx)
            {
                const UTruck truckStatus = worldRes.truckstatus(idx);
                std::thread thd(&UPS::handleTruckStatusQuery, this, truckStatus);
                thd.detach();
            }
        }
    }

public:
    UPS() : 
        errCount { 0 },
        worldSocket { nullptr },
        amazonSocket { nullptr },
        truckPool { new TruckPool },
        dbConn { new DatabaseLogger },
        seqGenerator { new SequenceGenerator }
        {}

    int connectWorld(const char * hostname, const char * port)
    {
        // send connect request to world
        bool sendWorldConnSuccuss = false;
        worldSocket = new Socket(hostname, port);
        while(!sendWorldConnSuccuss)
        {
            UConnect connectData = DataGenerator::getInstance()->genConnectWorldData(truckPool);
            sendWorldConnSuccuss = worldSocket->sendMsg(connectData);
            Logger::getInstance()->log("world.log", "Connect world message:\n", connectData.DebugString());
        }
        Logger::getInstance()->log("world.log", "Send connection request to world");

        // receive connection response from world
        bool recvWorldConnSuccess = false;
        UConnected connRes;
        while(!recvWorldConnSuccess)
        {
            recvWorldConnSuccess = worldSocket->recvMsg(connRes);
            Logger::getInstance()->log("world.log", connRes.worldid(), connRes.result());
        }
        
        Logger::getInstance()->log("world.log", "Connect world success");
        return connRes.worldid();
    }

    void connectAmazon(const char * hostname, const char * port, int worldid)
    {
        // connect to Amazon
        bool sendAmazonConnSuccess = false;
        amazonSocket = new Socket(hostname, port);
        unsigned connAmazonSeq = 0;
        while(!sendAmazonConnSuccess)
        {
            connAmazonSeq = seqGenerator->getSeqNumber();
            UtoAConnect connectData = DataGenerator::getInstance()->genUAConnectData(worldid, connAmazonSeq);
            UtoACommand connReq;
            DataGenerator::getInstance()->addUAConnectData(connReq, connectData);
            sendAmazonConnSuccess = amazonSocket->sendMsg(connReq);
            Logger::getInstance()->log("amazon.log", "Send connection request to Amazon");
        }

        // receive Amazon connection response, make sure connection success
        bool connSuc = false;
        AtoUCommand connAmazonRes;
        while(!connSuc)
        {
            bool recvSuc = amazonSocket->recvMsg(connAmazonRes);
            if(!recvSuc)
            {
                continue;
            }
            int ack_size = connAmazonRes.ack_size();
            for(int idx = 0; idx < ack_size; ++idx)
            {
                if(connAmazonRes.ack(idx) == connAmazonSeq)
                {
                    connSuc = true;
                    break;
                }
            }
        }
        Logger::getInstance()->log("amazon.log", "Connect Amazon success");
    }

    void run()
    {
        // create two detached threads to handle communication with Amazon and world
        std::thread amazonThread(&UPS::handleAmazonRes, this);
        amazonThread.detach();
        std::thread worldThread(&UPS::handleWorldRes, this);
        worldThread.detach();

        while(true)
        {
            // main thread is responsible for resend un-acknowledged message, and clear handled sequence number
            std::this_thread::sleep_for(std::chrono::seconds(SLEEP_PERIOD)); 
            seqGenerator->clearHandledRequest();
            seqGenerator->resendMessage(worldSocket, amazonSocket);

            /* DEBUG
            // query truck status to check if the world is running
            int truckCount = truckPool->getTruckCount();
            for(int idx = 0; idx < truckCount; ++idx)
            {
                queryTruck(idx);
            }
            */
        }
    }

    ~UPS() noexcept
    {
        delete(worldSocket);
        delete(amazonSocket);
        delete(truckPool);
        delete(dbConn);
        delete(seqGenerator);
    }

private:
    std::atomic<unsigned> errCount;
    Socket * worldSocket;
    Socket * amazonSocket;
    TruckPool * truckPool;
    DatabaseLogger * dbConn;
    SequenceGenerator * seqGenerator;
};

#endif