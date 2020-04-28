#ifndef TRUCKPOOL_HPP__
#define TRUCKPOOL_HPP__

#include "truck.hpp"
#include <mutex>
#include <queue>
#include <vector>
#include <string>
#include <unordered_map>
#include <condition_variable>
#define TRUCK_NUM 1024

class TruckPool
{
private:
    void setupTrucks()
    {
        for(int idx = 0; idx < TRUCK_NUM; ++idx)
        {
            trucks.emplace_back(0, 0);
            availableTrucks.push(idx++);
        }
    }

public:
    TruckPool()
    {
        setupTrucks();
    }

    unsigned getTruckCount() const { return trucks.size(); }

    const Truck & getTruck(int truckid) const { return trucks[truckid]; }

    void setWarehouseid(int truckid, int warehouseid) { trucks[truckid].setWarehouseid(warehouseid); }

    int getWarehouseid(int truckid) { return trucks[truckid].getWarehouseid(); }

    void addPackage(int truckid, int packageid) { trucks[truckid].addPackage(packageid); }

    const std::vector<int> getPackages(int truckid) { return trucks[truckid].getPackages(); }

    // (1) check if there's truck to the target warehouse
    // (2) if not, wait until there's free truck available
    int getFreeTruck(int warehouseid)
    {
        std::unique_lock<std::mutex> lck(mtx);
        if(truckToWarehouse.find(warehouseid) != truckToWarehouse.end())
        {
            return truckToWarehouse[warehouseid];
        }
        cv.wait(lck, [&](){ return !availableTrucks.empty(); });
        int truckid = availableTrucks.front(); availableTrucks.pop();
        return truckid;
    }

    // truck has departed to destinations, cannot be assigned
    void registerTruck(int truckid)
    {
        std::unique_lock<std::mutex> lck(mtx);
        truckToWarehouse.erase(getWarehouseid(truckid));
    }

    // truck has finish delivery
    void returnTruck(int truckid)
    {
        std::unique_lock<std::mutex> lck(mtx);
        trucks[truckid].reset();
        availableTrucks.push(truckid);
        cv.notify_one();
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    std::vector<Truck> trucks;
    std::queue<int> availableTrucks; // trucks available, not on the way to warehouse or destination
    std::unordered_map<int, int> truckToWarehouse; // warehouse id as key, truck id as value(only trucks on the way to warehouse)
};

#endif