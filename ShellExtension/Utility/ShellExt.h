#pragma once

#include <string>
#include <Windows.h>

namespace ShellExt {
    LONG DllAddRef();
    LONG DllRelease();
    BOOL DllReferenceCount();

    std::string GuidToString(const GUID& guid);
}

namespace ShellExt::Setup {
    DWORD Install(const std::string& module_path, const std::string& guid, const std::string& class_name);
    DWORD Uninstall(const std::string& guid);
}