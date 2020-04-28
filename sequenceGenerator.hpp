#ifndef SEQUENCE_GENERATOR_HPP__
#define SEQUENCE_GENERATOR_HPP__

#include "UA.pb.h"
#include "world_ups.pb.h"
#include "socket.hpp"
#include "logger.hpp"
#include "dataGenerator.hpp"
#include "threadsafe_unordered_map.hpp"
#include <mutex>
#include <queue>
#include <vector>
#include <thread>
#include <string>
#include <atomic>
#include <climits>
#include <cstdlib>

// sent_threshold and check_sent_exceed() is for resend un-acknowledged sent message
// every 30 second, if message not ack-ed, it will be re-sent
// recv_threshold and check_recv_exceed() is for avoid re-handling request, like access database
// every 120 minutes, it should be no communication problem(un-acked); then clear the record to save space 
#define SENT_THRESHOLD 30
#define RECV_THRESHOLD 120
#define CHECK_SENT_EXCEED(val1, val2) abs((val1) - (val2)) >= SENT_THRESHOLD
#define CHECK_RECV_EXCEED(val1, val2) abs((val1) - (val2)) >= RECV_THRESHOLD

class SequenceGenerator
{
private:
    using UGoPickup_with_time = std::pair<int, UGoPickup>;
    using UGoDeliver_with_time = std::pair<int, UGoDeliver>;
    using UQuery_with_time = std::pair<int, UQuery>;
    using UtoALoadRequest_with_time = std::pair<int, UtoALoadRequest>;
    using Delivery_with_time = std::pair<int, Delivery>;
    using UserValidationResponse_with_time = std::pair<int, UserValidationResponse>;

private:
    int getCurrentSecond() const
    {
        time_t cur = time(NULL);
        tm * tm = localtime(&cur);
        return tm->tm_sec;
    }

    int getTimestamp() const
    {
        time_t cur = time(NULL);
        tm * tm = localtime(&cur);
        int minVal = tm->tm_min;
        int secVal = tm->tm_sec;
        return minVal * 60 + secVal;
    }

    void resendAmazonMessage(Socket * amazonSocket, int cur_second)
    {
        UtoACommand toAmazonCommand;
        std::vector<UtoALoadRequest_with_time> unackedUtoALoadRequest = toAmazonLoadReqs.getAllValue();
        for(auto & msg : unackedUtoALoadRequest)
        {
            if(CHECK_SENT_EXCEED(msg.first, cur_second))
            {
                DataGenerator::getInstance()->addUtoALoadRequest(toAmazonCommand, msg.second);
            }
        }
        std::vector<Delivery_with_time> unackedDelivery = toAmazonDeliveries.getAllValue();
        for(auto & msg : unackedDelivery)
        {
            if(CHECK_SENT_EXCEED(msg.first, cur_second))
            {
                DataGenerator::getInstance()->addDelivery(toAmazonCommand, msg.second);
            }
        }
        std::vector<UserValidationResponse_with_time> unackedUserValidationResponse = toAmazonUserValidationReses.getAllValue();
        for(auto & msg : unackedUserValidationResponse)
        {
            if(CHECK_SENT_EXCEED(msg.first, cur_second))
            {
                DataGenerator::getInstance()->addUserValidationResponse(toAmazonCommand, msg.second);
            }
        }
        amazonSocket->sendMsg(toAmazonCommand);
        Logger::getInstance()->log("amazon.log", "Resend amazon message:\n", toAmazonCommand.DebugString());
    }

    void resendWorldMessage(Socket * worldSocket, int cur_second)
    {
        UCommands toWorldCommand;
        std::vector<UGoPickup_with_time> unackedUGoPickup = toWorldPickupReqs.getAllValue();
        for(auto & msg : unackedUGoPickup)
        {
            if(CHECK_SENT_EXCEED(msg.first, cur_second))
            {
                DataGenerator::getInstance()->addUGoPickup(toWorldCommand, msg.second);
            }
        }
        std::vector<UGoDeliver_with_time> unackedUGoDeliver = toWorldDeliverReqs.getAllValue();
        for(auto & msg : unackedUGoDeliver)
        {
            if(CHECK_SENT_EXCEED(msg.first, cur_second))
            {
                DataGenerator::getInstance()->addUGoDeliver(toWorldCommand, msg.second);
            }
        }
        std::vector<UQuery_with_time> unackedUQuery = toWorldQueryReqs.getAllValue();
        for(auto & msg : unackedUQuery)
        {
            if(CHECK_SENT_EXCEED(msg.first, cur_second))
            {
                DataGenerator::getInstance()->addUQuery(toWorldCommand, msg.second);
            }
        }
        worldSocket->sendMsg(toWorldCommand);
        Logger::getInstance()->log("world.log", "Resend to world message:\n", toWorldCommand.DebugString());
    }

public:
    SequenceGenerator() : 
        seq { 0 }
        {}

    // for UPS as send side, register every used sequence number
    unsigned getSeqNumber()
    {
        while(seq_sent.find(seq))
        {
            ++seq; // make sure sequence number is not occupied at present
        }
        seq_sent.insert(seq, 0); // initial value, act as place holder
        return seq++; 
    }

    // for UPS as send side, remove every used sequence number
    void receiveAck(unsigned ack)
    {
        if(!seq_sent.find(ack))
        {
            return;
        }

        unsigned msgType = seq_sent.get(ack);
        seq_sent.erase(ack);
        switch(msgType)
        {
            case TO_WORLD_PICKUP_REQ: toWorldPickupReqs.erase(ack); break;
            case TO_WORLD_DELIVER_REQ: toWorldDeliverReqs.erase(ack); break;
            case TO_WORLD_QUERY_REQ: toWorldQueryReqs.erase(ack); break;
            case TO_AMAZON_LOAD_REQ: toAmazonLoadReqs.erase(ack); break;
            case TO_AMAZON_DELIVERY: toAmazonDeliveries.erase(ack); break;
            case TO_AMAZON_USER_VALID: toAmazonUserValidationReses.erase(ack); break;
            default: break;
        }
    }

    // record every received/handle sequence number, in order to avoid re-handling
    void addHandledRequest(int seqNum)
    {
        seq_handled.insert(seqNum, getTimestamp());
    }

    // check if the sequence has been handled
    bool checkAlreadyHandled(int seqNum)
    {
        return seq_handled.find(seqNum);
    }

    // clear record to save space
    void clearHandledRequest()
    {
        int curTimestamp = getTimestamp();
        std::vector<unsigned> seqs = seq_handled.getAllKey();
        for(unsigned seq : seqs)
        {
            int temp_timestamp = seq_handled.get(seq);
            if(CHECK_RECV_EXCEED(curTimestamp, temp_timestamp))
            {
                seq_handled.erase(seq);
            }
        }
    }

    void addSentMessage(unsigned seqNum, const UGoPickup & toWorldPickupReq)
    {
        seq_sent.insert(seqNum, TO_WORLD_PICKUP_REQ);
        toWorldPickupReqs.insert(seqNum, { getCurrentSecond(), toWorldPickupReq });
    }

    void addSentMessage(unsigned seqNum, const UGoDeliver & toWorldDeliverReq)
    {
        seq_sent.insert(seqNum, TO_WORLD_DELIVER_REQ);
        toWorldDeliverReqs.insert(seqNum, { getCurrentSecond(), toWorldDeliverReq });
    }

    void addSentMessage(unsigned seqNum, const UtoALoadRequest & toAmazonLoadReq)
    {
        seq_sent.insert(seqNum, TO_AMAZON_LOAD_REQ);
        toAmazonLoadReqs.insert(seqNum, { getCurrentSecond(), toAmazonLoadReq });
    }

    void addSentMessage(unsigned seqNum, const Delivery & toAmazonDelivery)
    {
        seq_sent.insert(seqNum, TO_AMAZON_DELIVERY);
        toAmazonDeliveries.insert(seqNum, { getCurrentSecond(), toAmazonDelivery });
    }

    void addSentMessage(unsigned seqNum, const UserValidationResponse & toAmazonUserValidationRes)
    {
        seq_sent.insert(seqNum, TO_AMAZON_USER_VALID);
        toAmazonUserValidationReses.insert(seqNum, { getCurrentSecond(), toAmazonUserValidationRes });
    }

    void addSentMessage(unsigned seqNum, const UQuery & toWorldQueryReq)
    {
        seq_sent.insert(seqNum, TO_WORLD_QUERY_REQ);
        toWorldQueryReqs.insert(seqNum, { getCurrentSecond(), toWorldQueryReq });
    }

    void resendMessage(Socket * worldSocket, Socket * amazonSocket)
    {
        // Embarassingly Parallelism
        int cur_second = getCurrentSecond();
        std::thread amazonResendThread(&SequenceGenerator::resendAmazonMessage, this, amazonSocket, cur_second);
        amazonResendThread.detach();
        std::thread worldResendThread(&SequenceGenerator::resendWorldMessage, this, worldSocket, cur_second);
        worldResendThread.detach();
    }

private:
    // type of message
    enum
    {
        TO_WORLD_PICKUP_REQ = 0,
        TO_WORLD_DELIVER_REQ = 1,
        TO_WORLD_QUERY_REQ = 2,
        TO_AMAZON_LOAD_REQ = 3,
        TO_AMAZON_DELIVERY = 4,
        TO_AMAZON_USER_VALID = 5,
    };

    // sequence number related variable
    std::atomic<unsigned> seq;
    ThreadsafeUnorderedMap<unsigned, unsigned> seq_sent;
    ThreadsafeUnorderedMap<unsigned, unsigned> seq_handled;

    // sent message related container
    ThreadsafeUnorderedMap<unsigned, UGoPickup_with_time> toWorldPickupReqs;
    ThreadsafeUnorderedMap<unsigned, UGoDeliver_with_time> toWorldDeliverReqs;
    ThreadsafeUnorderedMap<unsigned, UQuery_with_time> toWorldQueryReqs;
    ThreadsafeUnorderedMap<unsigned, UtoALoadRequest_with_time> toAmazonLoadReqs;
    ThreadsafeUnorderedMap<unsigned, Delivery_with_time> toAmazonDeliveries;
    ThreadsafeUnorderedMap<unsigned, UserValidationResponse_with_time> toAmazonUserValidationReses;
};

#endif