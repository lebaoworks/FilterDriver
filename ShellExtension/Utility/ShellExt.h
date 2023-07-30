#pragma once

#include <string>
#include <Windows.h>

namespace ShellExt {
    LONG DllAddRef();
    LONG DllRelease();

    std::string GuidToString(const GUID& guid);
}