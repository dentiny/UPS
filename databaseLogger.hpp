#ifndef DATABASE_LOGGER_HPP__
#define DATABASE_LOGGER_HPP__

#include "UA.pb.h"
#include "world_ups.pb.h"
#include "logger.hpp"
#include "constants.hpp"
#include <array>
#include <mutex>
#include <ctime>
#include <string>
#include <cstdlib>
#include <exception>
#include <pqxx/pqxx>

class DatabaseLogger
{
private:
    std::string getCurrentTime() const
    {
        time_t raw_time;
        time(&raw_time);
        struct tm * cur_time = localtime(&raw_time);
        return std::string(asctime(cur_time));
    }

    int getUserId(const std::string & username)
    {
        try
        {
            pqxx::work W(*conn);
            std::string sql = std::string("SELECT id FROM auth_user WHERE username = ") + W.quote(username) + ";";
            pqxx::result res { W.exec(sql) };
            return res.begin()[0].as<int>();
        }
        catch(std::exception & e)
        {
            Logger::getInstance()->log("error.log", "Database error:", e.what());
            throw e;
        }
    }

public:
    DatabaseLogger()
    {
        std::string connect_sql = "dbname=upsdb user=postgres password=abc123";
        conn = new pqxx::connection(connect_sql);
        if(conn->is_open()) 
        {
            Logger::getInstance()->log("test.log", "Database connection success");
        } 
        else 
        {
            Logger::getInstance()->log("test.log", "Database connection failure");
            exit(EXIT_FAILURE);
        }
    }

    bool checkAccount(const std::string & account)
    {
        bool ret = false;
        try
        {
            std::unique_lock<std::mutex> lck(mtx);
            pqxx::work W(*conn);
            std::string sql = std::string("SELECT * FROM auth_user WHERE username = ") + W.quote(account) + ";";
            pqxx::result res { W.exec(sql) };
            ret = res.begin() != res.end();
        }
        catch(std::exception & e)
        {
            Logger::getInstance()->log("error.log", "Database error:", e.what());
        }
        return ret;
    }

    void createProduct(int packageId, const std::string description, int count)
    {
        try
        {
            std::unique_lock<std::mutex> lck(mtx);
            pqxx::work W(*conn);
            std::string sql = std::string("INSERT INTO ups_product (description, count, package_id) VALUES(")
                + W.quote(description) + "," + std::to_string(count) + "," + std::to_string(packageId) + ");"; 
            W.exec(sql);
            W.commit();
        }
        catch(std::exception & e)
        {
            Logger::getInstance()->log("error.log", "Database error:", e.what());
            throw e;
        }
    }

    void createPkg(int packageid, int dest_x, int dest_y)
    {
        try
        {
            std::unique_lock<std::mutex> lck(mtx);
            std::string cur_time = getCurrentTime();
            pqxx::work W(*conn);
            std::string sql = std::string("INSERT INTO ups_package (tracking_num, delivery_x, delivery_y, status, creation_time) VALUES(")
                + std::to_string(packageid) + "," + std::to_string(dest_x) + "," + std::to_string(dest_y) + 
                + "," + W.quote(states[CREATED]) + "," + W.quote(cur_time) + ");"; 
            W.exec(sql);
            W.commit();
        }
        catch(std::exception & e)
        {
            Logger::getInstance()->log("error.log", "Database error:", e.what());
            throw e;
        }
    }

    void createPkg(int packageId, int dest_x, int dest_y, const std::string & username)
    {
        try
        {
            std::unique_lock<std::mutex> lck(mtx);
            std::string cur_time = getCurrentTime();
            int user_id = getUserId(username);
            pqxx::work W(*conn);
            std::string sql = std::string("INSERT INTO ups_package (tracking_num, delivery_x, delivery_y, user_id, status, creation_time) VALUES(")
                + std::to_string(packageId) + "," + std::to_string(dest_x) + "," + std::to_string(dest_y) + ", " 
                + std::to_string(user_id) + "," + W.quote(states[CREATED]) + "," + W.quote(cur_time) + ");"; 
            W.exec(sql);
            W.commit();
        }
        catch(std::exception & e)
        {
            Logger::getInstance()->log("error.log", "Database error:", e.what());
            throw e;
        }
    }
    
    void updatePkgState(int packageId, int newState)
    {
        try
        {
            std::unique_lock<std::mutex> lck(mtx);
            std::string state = states[newState];
            pqxx::work W(*conn);
            std::string sql = std::string("UPDATE ups_package ")
                + "SET status = " + W.quote(state)
                + "WHERE tracking_num = " + std::to_string(packageId) + ";";

            // update 
            std::string cur_time = getCurrentTime();
            if (newState == OUT_FOR_DELIVERY) 
            {
                sql += std::string("UPDATE ups_package ") + "SET pickup_time = " + W.quote(cur_time) + ";";
            } 
            else if (newState == DELIVERED) 
            {
                sql += std::string("UPDATE ups_package ") + "SET delivered_time = " + W.quote(cur_time) + ";";
            }
            
            W.exec(sql);
            W.commit();
        }
        catch(std::exception & e)
        {
            Logger::getInstance()->log("error.log", "Database error:", e.what());
            throw e;
        }
    }

    UDeliveryLocation getPackage(int packageId)
    {
        UDeliveryLocation curPackage;
        try
        {
            std::unique_lock<std::mutex> lck(mtx);
            pqxx::work W(*conn);
            std::string sql = std::string("SELECT * FROM ups_package WHERE tracking_num = ") + std::to_string(packageId) + ";";
            pqxx::result res { W.exec(sql) };
            curPackage.set_packageid(packageId);
            curPackage.set_x(res.begin()[1].as<int>());
            curPackage.set_y(res.begin()[2].as<int>());
        }
        catch(std::exception & e)
        {
            Logger::getInstance()->log("error.log", "Database error:", e.what());
            throw e;
        }
        return curPackage;
    }

    ~DatabaseLogger() noexcept
    {
        if(conn->is_open())
        {
            conn->disconnect();
        }
        delete(conn);
    }

private:
    std::mutex mtx; // cannot read/write database at the same time, need synchronization
    pqxx::connection * conn;

    std::array<std::string, 5> states = 
    {
        "Created",
        "Truck en route to warehouse",
        "Truck waiting for package",
        "Out for delivery",
        "Delivered"
    };  
};

#endif