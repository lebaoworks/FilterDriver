#pragma once

#include <string>
#include <Windows.h>

namespace Win32 {
    namespace Registry {
        LSTATUS OpenKey(const std::string& key, HKEY& handle);
        LSTATUS CreateKey(const std::string& key);
        LSTATUS DeleteKey(const std::string& key);
        LSTATUS SetValueString(const std::string& key, const std::string& value, const std::string& data);

        LSTATUS CreateKey(const std::string& key, const std::string& default_value);
    }
    namespace Module {
        std::string GetCurrentModulePath();
    }
    
}