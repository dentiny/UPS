#include "UA.pb.h"
#include "world_ups.pb.h"
#include "UPS.hpp"
#include "logger.hpp"
#include <cstdlib>
#include <iostream>

int main()
{
    Logger::getInstance()->clearLog();
    UPS ups;
    int worldid = ups.connectWorld("127.0.0.1", "12345");
    ups.connectAmazon("127.0.0.1", "5555", worldid);
    ups.run();
    return EXIT_SUCCESS;
}