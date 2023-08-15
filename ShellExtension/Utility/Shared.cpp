#include "Shared.h"

/*********************
*       Logging      *
*********************/
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <sstream>

#include <Windows.h>

#ifndef _DEBUG
auto fo = std::ofstream(R"(C:\Users\test\Desktop\Artifact\log.txt)", std::ios::app);
#endif

namespace shared::logging
{
    static Level GlobalLevel = Level::Info;

    void log(_In_ Level level, _In_z_ const char* szString)
    {
        if (level < GlobalLevel)
            return;

        std::time_t t = std::time(nullptr);
        std::tm tm;
        localtime_s(&tm, &t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%c");

        std::string prefix = "[Unknown]";
        if (level == Level::Debug)
            prefix = "Debug";
        else if (level == Level::Info)
            prefix = "Info";
        else if (level == Level::Warning)
            prefix = "Warning";
        else if (level == Level::Error)
            prefix = "Error";
        else
            prefix = "Unknown";

        auto output = shared::format("[%s] [PID: %5d] [TID: %5d] [%s] %s", ss.str().c_str(), GetCurrentProcessId(), GetCurrentThreadId(), prefix.c_str(), szString);

#ifdef _DEBUG
        std::cout << output;
#else
        fo << output;
        fo.flush();
#endif
    }
}


