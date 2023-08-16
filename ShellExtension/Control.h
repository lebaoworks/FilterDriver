#pragma once

#include <vector>
#include <string>

namespace Control
{
    bool Protect(const std::vector<std::wstring>& paths) noexcept;
    bool Unprotect(const std::vector<std::wstring>& paths) noexcept;
}

