#ifndef DATA_GENERATOR_HPP__
#define DATA_GENERATOR_HPP__

#include "UA.pb.h"
#include "world_ups.pb.h"
#include "truck.hpp"
#include "truckpool.hpp"
#include <vector>
#include <iostream>
#define SIMSPEED 100

class DataGenerator
{
private:
    DataGenerator() {}

public:
    static DataGenerator * getInstance()
    {
        static DataGenerator * dataGenerator;
        return dataGenerator != nullptr ? dataGenerator : new DataGenerator();
    }

    void clearUtoACommand(UtoACommand & toAmazonCommand)
    {
        toAmazonCommand.clear_connection();
        toAmazonCommand.clear_usrvlid();
        toAmazonCommand.clear_loadreq();
        toAmazonCommand.clear_delivery();
        toAmazonCommand.clear_errmsg();
        toAmazonCommand.clear_ack();
        toAmazonCommand.clear_disconnection();
    }

    void clearUCommands(UCommands & toWorldCommand)
    {
        toWorldCommand.clear_pickups();
        toWorldCommand.clear_deliveries();
        toWorldCommand.clear_simspeed();
        toWorldCommand.clear_disconnect();
        toWorldCommand.clear_queries();
        toWorldCommand.clear_acks();
    }

    // add simspeed to UCommands
    void addSimspeed(UCommands & worldCommand)
    {
        worldCommand.set_simspeed(SIMSPEED);
    }

    // add sequence number to UtoACommand
    void addSeqNumberToAmazonCommand(UtoACommand & toAmazonAckCommand, unsigned seqNum)
    {
        toAmazonAckCommand.add_ack(seqNum);
    }

    // add sequence number to UCommands
    void addSeqNumberToWorldCommand(UCommands & toWorldAckCommand, unsigned seqNum)
    {
        toWorldAckCommand.add_acks(seqNum);
    }

    // generate UConnect data
    UConnect genConnectWorldData(const TruckPool * tp)
    {
        UConnect connReq;
        connReq.set_isamazon(false);
        int truckCnt = tp->getTruckCount();
        for(int idx = 0; idx < truckCnt; ++idx)
        {
            const auto & truck = tp->getTruck(idx);
            UInitTruck * truckObj = connReq.add_trucks();
            truckObj->set_id(idx);
            truckObj->set_x(truck.getInitX());
            truckObj->set_y(truck.getInitY());
        }
        return connReq;
    }

    // generate UtoAConnnect data
    UtoAConnect genUAConnectData(int worldid, unsigned seqNum)
    {
        UtoAConnect connReq;
        connReq.set_seqnum(seqNum);
        connReq.set_worldid(worldid);
        return connReq;
    }

    // add UtoAConnect to UtoACommand
    void addUAConnectData(UtoACommand & msg, UtoAConnect & connectData)
    {
        UtoAConnect * temp_connectData = msg.add_connection();
        *temp_connectData = connectData;
    }

    // generate UserValidationResponse
    UserValidationResponse genUserValidationResponse(bool isAccountValid, int shipid, int seqNum)
    {
        UserValidationResponse toAmazonUserValidationRes;
        toAmazonUserValidationRes.set_seqnum(seqNum);
        toAmazonUserValidationRes.set_shipid(shipid);
        toAmazonUserValidationRes.set_result(isAccountValid);
        return toAmazonUserValidationRes;
    }

    // add UserValidationResponse to UtoACommand
    void addUserValidationResponse(UtoACommand & toAmazonUserValidationResCommand, UserValidationResponse & toAmazonUserValidationRes)
    {
        UserValidationResponse * temp_toAmazonUserValidationRes = toAmazonUserValidationResCommand.add_usrvlid();
        *temp_toAmazonUserValidationRes = toAmazonUserValidationRes;
    }

    // generate UGoPickup
    UGoPickup genUGoPickup(int truckId, int warehouseId, int seqNum)
    {
        UGoPickup toWorldPickupReq;
        toWorldPickupReq.set_truckid(truckId);
        toWorldPickupReq.set_whid(warehouseId);
        toWorldPickupReq.set_seqnum(seqNum);
        return toWorldPickupReq;
    }

    // add UGoPickup to UCommand
    void addUGoPickup(UCommands & toWorldPickupReqCommand, UGoPickup & toWorldPickupReq)
    {
        UGoPickup * temp_toWorldPickupReq = toWorldPickupReqCommand.add_pickups();
        *temp_toWorldPickupReq = toWorldPickupReq;
    }

    // generate UGoDeliver
    UGoDeliver genUGoDeliver(int truckId, std::vector<UDeliveryLocation> & packages, int seqNum)
    {
        UGoDeliver toWorldDeliverReq;
        toWorldDeliverReq.set_truckid(truckId);
        toWorldDeliverReq.set_seqnum(seqNum);
        for(auto & package : packages)
        {
            UDeliveryLocation * temp_package = toWorldDeliverReq.add_packages();
            *temp_package = package;
        }
        return toWorldDeliverReq;
    }

    // add UGoDeliver to UCommand
    void addUGoDeliver(UCommands & toWorldDeliverReqCommand, UGoDeliver & toWorldDeliverReq)
    {
        UGoDeliver * temp_toWorldDeliverReq = toWorldDeliverReqCommand.add_deliveries();
        *temp_toWorldDeliverReq = toWorldDeliverReq;
    }

    // generate UtoALoadRequest
    UtoALoadRequest genUtoALoadRequest(int truckid, int warehouseid, const std::vector<int> & packages, int seqNum)
    {
        UtoALoadRequest toAmazonLoadReq;
        toAmazonLoadReq.set_truckid(truckid);
        toAmazonLoadReq.set_warehouseid(warehouseid);
        toAmazonLoadReq.set_seqnum(seqNum);
        for(int packageid : packages)
        {
            toAmazonLoadReq.add_shipid(packageid);
        }
        return toAmazonLoadReq;
    }

    // add UtoALoadRequest to UtoACommand
    void addUtoALoadRequest(UtoACommand & toAmazonLoadReqCommand, UtoALoadRequest & toAmazonLoadReq)
    {
        UtoALoadRequest * temp_toAmazonLoadReq = toAmazonLoadReqCommand.add_loadreq();
        *temp_toAmazonLoadReq = toAmazonLoadReq;
    }

    // generate Delivery
    Delivery genDelivery(int packageid, int seqnum)
    {
        Delivery toAmazonDelivery;
        toAmazonDelivery.set_shipid(packageid);
        toAmazonDelivery.set_seqnum(seqnum);
        return toAmazonDelivery;
    }

    // add Delivery to UtoACommand
    void addDelivery(UtoACommand & toAmazonDeliveryCommand, Delivery & toAmazonDelivery)
    {
        Delivery * temp_delivery = toAmazonDeliveryCommand.add_delivery();
        *temp_delivery = toAmazonDelivery;
    }

    // generate UQuery
    UQuery genUQuery(int truckid, int seqnum)
    {
        UQuery query;
        query.set_truckid(truckid);
        query.set_seqnum(seqnum);
        return query;
    }

    // add UQuery to UCommands
    void addUQuery(UCommands & worldCommand, UQuery & query)
    {
        UQuery * temp_query = worldCommand.add_queries();
        *temp_query = query;
    }
};

#endif