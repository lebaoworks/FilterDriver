#include "Shared.h"

/*********************
*       Logging      *
*********************/
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>

//auto fo = std::ofstream(R"(C:\Users\test\Desktop\Artifact\log.txt)", std::ios::app);

void shared::log(_In_ unsigned int level, _In_z_ const char* szString)
{
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &t);
    printf("%s", szString);
    std::cout << "[" << std::put_time(&tm, "%c") << "] " << szString;
}

