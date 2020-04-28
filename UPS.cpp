#include "UPS.hpp"
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

// naming of the local variable:
// to world/Amazon verb(pickup, load, ...) req
// to world/Amazon verb(pickup, load, ...) reqCommand
// from world/Amazon verb(pickup, load, ...) res
// from world/Amazon verb(pickup, load, ...) resCommand

void UPS::queryTruck(int truckid)
{
    try
    {
        // (1) generate UQuery
        // (2) add UQuery to UCommands
        // (3) send UCommands to world
        // (4) record sent message
        int seqNum = seqGenerator->getSeqNumber();
        UQuery toWorldQueryTruckReq = DataGenerator::getInstance()->genUQuery(truckid, seqNum);
        UCommands toWorldQueryTruckReqCommand;
        DataGenerator::getInstance()->addUQuery(toWorldQueryTruckReqCommand, toWorldQueryTruckReq);
        worldSocket->sendMsg(toWorldQueryTruckReqCommand);
        seqGenerator->addSentMessage(seqNum, toWorldQueryTruckReq);
        Logger::getInstance()->log("world.log", "Send world to query truck status:\n", toWorldQueryTruckReqCommand.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.log", "queryTruck() error");
    }
}

void UPS::handleTruckStatusQuery(const UTruck truckStatusQueryRes)
{
    try
    {
        // check received sequence number, or send ack message to Amazon
        int recv_seq = truckStatusQueryRes.seqnum();
        if(seqGenerator->checkAlreadyHandled(recv_seq))
        {
            std::cout << "Sequence number " << recv_seq << " already handled" << std::endl;
            return;
        }
        sendAckMessageToWorld(recv_seq);
        seqGenerator->addHandledRequest(recv_seq);
        Logger::getInstance()->log("world.txt", "Receive truck status query result:\n", truckStatusQueryRes.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "handleTruckStatusQuery() error");
    }
}

void UPS::sendAckMessageToAmazon(unsigned seqNum)
{
    try
    {
        UtoACommand toAmazonAckCommand;
        DataGenerator::getInstance()->addSeqNumberToAmazonCommand(toAmazonAckCommand, seqNum);
        amazonSocket->sendMsg(toAmazonAckCommand);
        Logger::getInstance()->log("amazon.log", "send ack to amazon\n", toAmazonAckCommand.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "sendAckMessageToAmazon() error");
    }
}

void UPS::sendAckMessageToWorld(unsigned seqNum)
{
    try
    {
        UCommands toWorldAckCommand;
        DataGenerator::getInstance()->addSeqNumberToWorldCommand(toWorldAckCommand, seqNum);
        worldSocket->sendMsg(toWorldAckCommand);
        Logger::getInstance()->log("world.log", "send ack to world\n", toWorldAckCommand.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "sendAckMessageToWorld() error");
    }
}

// receive user validation request from Amazon
void UPS::handleUserValidationReq(const UserValidationRequest fromAmazonUserValidationReq)
{
    try
    {
        Logger::getInstance()->log("amazon.log", "Received from Amazon on user validation:\n", fromAmazonUserValidationReq.DebugString());

        // check received sequence number, or send ack message to Amazon
        int recv_seq = fromAmazonUserValidationReq.seqnum();
        if(seqGenerator->checkAlreadyHandled(recv_seq))
        {
            std::cout << "Sequence number " << recv_seq << " already handled" << std::endl;
            return;
        }
        sendAckMessageToAmazon(recv_seq);
        seqGenerator->addHandledRequest(recv_seq);
        
        // (1) generate UserValidationResponse
        // (2) add UserValidationResponse to AtoUCommand
        // (3) send AtoUCommand to Amazon
        // (4) send ack message to Amazon
        // (5) record handled message
        // (6) record sent message
        std::string account = fromAmazonUserValidationReq.upsaccount();
        int shipid = fromAmazonUserValidationReq.shipid();
        bool isAccountValid = dbConn->checkAccount(account);
        int seqNum = seqGenerator->getSeqNumber();
        UserValidationResponse toAmazonUserValidationRes = DataGenerator::getInstance()->genUserValidationResponse(isAccountValid, shipid, seqNum);
        UtoACommand toAmazonUserValidationResCommand;
        DataGenerator::getInstance()->addUserValidationResponse(toAmazonUserValidationResCommand, toAmazonUserValidationRes);
        
        // DEBUG
        std::cout << "ready to send message to world at handleUserValidationReq\n============================================================" << std::endl;
        std::cout << toAmazonUserValidationResCommand.DebugString() << std::endl;

        amazonSocket->sendMsg(toAmazonUserValidationResCommand);
        seqGenerator->addSentMessage(seqNum, toAmazonUserValidationRes);
        Logger::getInstance()->log("amazon.log", "Respond to Amazon user validation request:\n", toAmazonUserValidationResCommand.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "sendAckMessageToWorld() error");
    }   
}

// receive from Amazon to pick up, ask world to pick up
void UPS::handlePickupReq(const AtoUPickupRequest fromAmazonPickUpReq)
{
    try
    {
        Logger::getInstance()->log("amazon.log", "Received from Amazon on pick up\n", fromAmazonPickUpReq.DebugString());

        // check received sequence number, or send ack message to Amazon
        int recv_seq = fromAmazonPickUpReq.seqnum();
        if(seqGenerator->checkAlreadyHandled(recv_seq))
        {
            std::cout << "Sequence number " << recv_seq << " already handled" << std::endl;
            return;
        }
        sendAckMessageToAmazon(recv_seq);
        seqGenerator->addHandledRequest(recv_seq);
        
        // (1) generate UGoPickup
        // (2) set truck status: warehouse id and package id
        // (3) add simulation speed to UCommands
        // (4) add UGoPickup to UCommands
        // (5) send Ucommands to world
        // (6) send ack message to Amazon
        // (7) record handled message
        // (8) record sent message
        int warehouseid = fromAmazonPickUpReq.warehouseid();
        int truckid = truckPool->getFreeTruck(warehouseid);
        int seqNum = seqGenerator->getSeqNumber();
        UGoPickup toWorldPickupReq = DataGenerator::getInstance()->genUGoPickup(truckid, warehouseid, seqNum);
        truckPool->setWarehouseid(truckid, warehouseid);
        UCommands toWorldPickupReqCommand;
        DataGenerator::getInstance()->addSimspeed(toWorldPickupReqCommand);
        DataGenerator::getInstance()->addUGoPickup(toWorldPickupReqCommand, toWorldPickupReq);

        // DEBUG
        std::cout << "ready to send message to world at handlePickupReq\n============================================================" << std::endl;
        std::cout << toWorldPickupReqCommand.DebugString() << std::endl;

        worldSocket->sendMsg(toWorldPickupReqCommand);
        seqGenerator->addSentMessage(seqNum, toWorldPickupReq);

        // log into database
        int packageCnt = fromAmazonPickUpReq.shipment_size();
        for(int idx1 = 0; idx1 < packageCnt; ++idx1)
        {
            ShipInfo shipment = fromAmazonPickUpReq.shipment(idx1);
            int shipid = shipment.shipid();
            int dest_x = shipment.destination_x();
            int dest_y = shipment.destination_y();

            if(shipment.has_upsaccount())
            {
                std::string upsaccount = shipment.upsaccount();
                dbConn->createPkg(shipid, dest_x, dest_y, upsaccount);
            }
            else
            {
                dbConn->createPkg(shipid, dest_x, dest_y);
            }
            dbConn->updatePkgState(shipid, TRUCK_EN_ROUTE);
            truckPool->addPackage(truckid, shipid);

            int productCnt = shipment.products_size();
            for(int idx2 = 0; idx2 < productCnt; ++idx2)
            {
                Product product = shipment.products(idx2);
                std::string description = product.description();
                int count = product.count();
                dbConn->createProduct(shipid, description, count);
            }
        }

        Logger::getInstance()->log("world.log", "Send to world on pick up:\n", toWorldPickupReqCommand.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.log", "handlePickupReq() error");
    }
}

// receive from Amazon load has completed, ask world to deliver
void UPS::handleDeliveryReq(const AtoULoadFinishRequest fromAmazonDeliverReq)
{
    try
    {
        Logger::getInstance()->log("amazon.log", "Received from Amazon on load finish:\n", fromAmazonDeliverReq.DebugString());

        // check received sequence number, or send ack message to Amazon
        int recv_seq = fromAmazonDeliverReq.seqnum();
        if(seqGenerator->checkAlreadyHandled(recv_seq))
        {
            std::cout << "Sequence number " << recv_seq << " already handled" << std::endl;
            return;
        }
        sendAckMessageToAmazon(recv_seq);
        seqGenerator->addHandledRequest(recv_seq);

        // (1) generate UDeliveryLocation
        // (2) generate UGoDeliver
        // (3) add simulation speed to UCommands
        // (4) add UGoDeliver UCommands
        // (5) send Ucommands to world
        // (6) send ack message to Amazon
        // (7) record sent message
        // (8) record handled message
        int truckid = fromAmazonDeliverReq.truckid();
        int seqNum = seqGenerator->getSeqNumber();
        std::vector<int> package_ids;
        int N = fromAmazonDeliverReq.shipid_size();
        for(int idx = 0; idx < N; ++idx)
        {
            package_ids.push_back(fromAmazonDeliverReq.shipid(idx));
        }
        std::vector<UDeliveryLocation> packages;
        for(int packageid : package_ids)
        {
           packages.push_back(dbConn->getPackage(packageid));
        }
        UGoDeliver toWorldDeliverReq = DataGenerator::getInstance()->genUGoDeliver(truckid, packages, seqNum);
        UCommands toWorldDeliverReqCommand;
        DataGenerator::getInstance()->addSimspeed(toWorldDeliverReqCommand);
        DataGenerator::getInstance()->addUGoDeliver(toWorldDeliverReqCommand, toWorldDeliverReq);
        
        // DEBUG
        std::cout << "ready to send message to world at handleDeliveryReq\n============================================================" << std::endl;
        std::cout << toWorldDeliverReqCommand.DebugString() << std::endl;
        
        worldSocket->sendMsg(toWorldDeliverReqCommand);
        seqGenerator->addSentMessage(seqNum, toWorldDeliverReq);

        // update database
        for(int idx = 0; idx < fromAmazonDeliverReq.shipid_size(); ++idx)
        {
            int shipid = fromAmazonDeliverReq.shipid(idx);
            dbConn->updatePkgState(shipid, OUT_FOR_DELIVERY);
        }
        
        Logger::getInstance()->log("world.log", "Send to world on delivery:\n", toWorldDeliverReqCommand.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "handleDeliveryReq() error");
    }   
}

void UPS::handleAmazonErrMsg(const ErrorMessage errMsg)
{
    try
    {
        int recv_seq = errMsg.seqnum();
        if(seqGenerator->checkAlreadyHandled(recv_seq))
        {
            std::cout << "Sequence number " << recv_seq << " already handled" << std::endl;
            return;
        }
        sendAckMessageToAmazon(recv_seq);
        seqGenerator->addHandledRequest(recv_seq);
        Logger::getInstance()->log("error.log", "Amazon error message:\n", errMsg.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "handleAmazonErrMsg() error");
    }
}

void UPS::handleWorldErrMsg(const UErr errMsg)
{
    try
    {
        int recv_seq = errMsg.seqnum();
        if(seqGenerator->checkAlreadyHandled(recv_seq))
        {
            std::cout << "Sequence number " << recv_seq << " already handled" << std::endl;
            return;
        }
        sendAckMessageToWorld(recv_seq);
        seqGenerator->addHandledRequest(recv_seq);
        Logger::getInstance()->log("error.log", "World error message:\n", errMsg.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "handleWorldErrMsg() error");
    }
}

// there're two possiblities for receiving UFinished
// (1) world notify the truck arrive the warehouse, ask Amazon to load
// (2) world notify the truck finished all shipments, return truck to truck pool
void UPS::handleLoadReq(const UFinished fromWorldToLoadReq)
{
    try
    {
        Logger::getInstance()->log("world.log", "Received from world arrive warehouse:\n", fromWorldToLoadReq.DebugString());

        // check received sequence number, or send ack message to world
        int recv_seq = fromWorldToLoadReq.seqnum();
        if(seqGenerator->checkAlreadyHandled(recv_seq))
        {
            std::cout << "Sequence number " << recv_seq << " already handled" << std::endl;
            return;
        }
        sendAckMessageToWorld(recv_seq);
        seqGenerator->addHandledRequest(recv_seq);

        // decide whether UFinished is used for
        // if used for notification of all deliveries for the truck, return the truck
        int truckid = fromWorldToLoadReq.truckid();
        std::string truckStatus = fromWorldToLoadReq.status();
        if(truckStatus == "IDLE") // indicate all deliveries are made for the truck
        {
            truckPool->returnTruck(truckid);
            std::cout << "Truck " << truckid << " has made all its deliveries" << std::endl;
            Logger::getInstance()->log("world.log", "Truck ", truckid, " has made all its deliveries");
            return;
        }

        // the truck has arrived the warehouse, and ask Amazon to load packages
        // (1) register the truck, it cannot hold other packages
        // (2) generate UtoALoadRequest
        // (3) add UtoALoadRequest to UtoACommand
        // (4) send UtoACommand to Amazon
        // (5) update database status
        // (6) send ack message to world
        // (7) record handled message
        // (8) record sent message
        truckPool->registerTruck(truckid);
        int warehouseid = truckPool->getWarehouseid(truckid);
        int seqNum = seqGenerator->getSeqNumber();
        const std::vector<int> packages = truckPool->getPackages(truckid);
        UtoALoadRequest toAmazonLoadReq = DataGenerator::getInstance()->genUtoALoadRequest(truckid, warehouseid, packages, seqNum);
        UtoACommand toAmazonLoadReqCommand;
        DataGenerator::getInstance()->addUtoALoadRequest(toAmazonLoadReqCommand, toAmazonLoadReq);
        amazonSocket->sendMsg(toAmazonLoadReqCommand);
        seqGenerator->addSentMessage(seqNum, toAmazonLoadReq);

        // update database
        for(int packageid : packages)
        {
            dbConn->updatePkgState(packageid, TRUCK_WAITING);
        }
        
        Logger::getInstance()->log("amazon.log", "Send to Amazon on load:\n", toAmazonLoadReqCommand.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "handleLoadReq() error");
    }
}

// world delivery made, notify Amazon success
// UDeliveryMade is different with UFinished:
// UDeliveryMade is used when any one delivery is made
// UFinished is sent when all the deliveries are made for a truck
void UPS::handleDeliveryMadeRes(const UDeliveryMade fromWorldDeliveryMade)
{
    try
    {
        Logger::getInstance()->log("world.log", "Received from world delivery complete:\n", fromWorldDeliveryMade.DebugString());
    
        // check received sequence number, or send ack message to world
        int recv_seq = fromWorldDeliveryMade.seqnum();
        if(seqGenerator->checkAlreadyHandled(recv_seq))
        {
            std::cout << "Sequence number " << recv_seq << " already handled" << std::endl;
            return;
        }
        sendAckMessageToWorld(recv_seq);
        seqGenerator->addHandledRequest(recv_seq);

        // (1) generate Delivery
        // (2) add Delivery to UtoACommand
        // (3) send UtoACommand to Amazon
        // (4) update database status
        // (5) send ack message to world
        // (6) record handled message
        // (7) record sent message
        int packageid = fromWorldDeliveryMade.packageid();
        int seqnum = seqGenerator->getSeqNumber();
        dbConn->updatePkgState(packageid, DELIVERED);  
        Delivery toAmazonDelivery = DataGenerator::getInstance()->genDelivery(packageid, seqnum);
        UtoACommand toAmazonDeliveryCommand;
        DataGenerator::getInstance()->addDelivery(toAmazonDeliveryCommand, toAmazonDelivery);
        amazonSocket->sendMsg(toAmazonDeliveryCommand);
        seqGenerator->addSentMessage(seqnum, toAmazonDelivery);

        Logger::getInstance()->log("amazon.log", "Send to Amazon on delivery:\n", toAmazonDeliveryCommand.DebugString());
    }
    catch(std::exception & e)
    {
        Logger::getInstance()->log("error.txt", "handleDeliveryMadeRes() error");
    }
}

void UPS::handleAck(int ack)
{
    Logger::getInstance()->log("amazon.log", "Receive ack:", ack);
    Logger::getInstance()->log("world.log", "Receive ack:", ack);
    seqGenerator->receiveAck(ack);
}