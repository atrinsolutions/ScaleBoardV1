#pragma once
#include <string>

class Logger {
public:
    static void init();
    static void info(const char* msg);
    static void error(const char* msg);
};
