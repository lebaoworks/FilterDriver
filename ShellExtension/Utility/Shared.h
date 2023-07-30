#pragma once

#include <Windows.h>
#include <sal.h>


/*********************
*       String       *
*********************/
#include <memory>
#include <string>
#include <stdexcept>

namespace shared {
    template<typename... Args>
    std::string format(const std::string& format, const Args&... args)
    {
        int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1;
        if (size_s <= 0)
            throw std::runtime_error("Error during formatting.");
        auto size = static_cast<size_t>(size_s);
        auto buf = std::make_unique<char[]>(size);
        std::snprintf(buf.get(), size, format.c_str(), args...);
        return std::string(buf.get(), buf.get() + size - 1);
    }
}


/*********************
*       Logging      *
*********************/

namespace shared {
    void log(_In_ unsigned int level, _In_z_ const char* szString);

    template<typename... T>
    void log(_In_ unsigned int level, _In_z_ const char* format, T... args)
    {
        log(level, shared::format(format, args...).c_str());
    }
}

#define Log(format, ...) shared::log(0, __FUNCTION__"() "##format"\n", __VA_ARGS__)


/*********************
*        Others      *
*********************/

// Source: https://stackoverflow.com/a/42060129
// Modified for rvalue
struct defer_dummy {};
template<class F>
struct deferer
{
    F _f;
    deferer(F&& f) : _f(f) {}
    ~deferer() { _f(); }
};
template<class F>
inline deferer<F> operator*(defer_dummy, F&& f)
{
    return deferer<F>(std::move(f));
}
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()