#ifndef TRUCK_HPP__
#define TRUCK_HPP__

#include <mutex>
#include <vector>

class Truck
{
public:
    Truck(int x, int y) :
        init_x { x },
        init_y { y },
        warehouseid { -1 }
        {}

    Truck(const Truck & truck) :
        init_x { truck.init_x },
        init_y { truck.init_y },
        warehouseid { truck.warehouseid },
        packages { truck.packages }
        {}

    void reset()
    {
        warehouseid = -1;
        packages.clear();
    }

    int getInitX() const { return init_x; }

    int getInitY() const { return init_y; }

    void setWarehouseid(int _warehouseid) { warehouseid = _warehouseid; }

    int getWarehouseid() const { return warehouseid; }

    const std::vector<int> & getPackages() { return packages; }

    void addPackage(int packageid) 
    { 
        std::unique_lock<std::mutex> lck(mtx);
        packages.push_back(packageid); 
    }

private:
    std::mutex mtx;
    const int init_x;
    const int init_y;
    int warehouseid;
    std::vector<int> packages;
};

#endif