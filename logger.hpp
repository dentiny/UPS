#ifndef LOGGER_HPP__
#define LOGGER_HPP__

#include <mutex>
#include <string>
#include <fstream>
#include <iostream>

class Logger
{
private:
    Logger() {}

    template<typename T>
    void logHelper(std::ofstream & of, T msg)
    {
        of << msg << std::endl;
        std::cout << msg << std::endl;
    }

    template<typename T, typename ... Types>
    void logHelper(std::ofstream & of, T msg, Types ... other)
    {
        of << msg;
        std::cout << msg;
        logHelper(of, other...);
    }

public:
    static Logger * getInstance()
    {
        std::mutex mtx;
        std::unique_lock<std::mutex> lck(mtx);
        static Logger * logger;
        return logger != nullptr ? logger : new Logger();
    }

    void clearLog()
    {
        of.open("amazon.log",std::ofstream::out);
        of.close();
        of.open("world.log",std::ofstream::out);
        of.close();
        of.open("error.log",std::ofstream::out);
        of.close();
    }

    template<typename ... Types>
    void log(std::string filename, Types ... content)
    {
        std::mutex mtx;
        std::unique_lock<std::mutex> lck(mtx);
        if(!of.is_open())
        {
            of.open(filename, std::ofstream::out | std::ofstream::app);
        }
        logHelper(of, content...);
        of.close();
    }

private:
    std::ofstream of;
};

#endif