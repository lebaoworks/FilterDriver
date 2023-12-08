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
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3

namespace shared::logging
{
    enum class Level
    {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    };

    void log(_In_ Level level, _In_z_ const char* szString);

    template<typename... T>
    void log(_In_ Level level, _In_z_ const char* format, T... args)
    {
        log(level, shared::format(format, args...).c_str());
    }
}

#define Log(format, ...)        shared::logging::log(shared::logging::Level::Info,    __FUNCTION__"() "##format"\n", __VA_ARGS__)
#define LogDebug(format, ...)   shared::logging::log(shared::logging::Level::Debug,   __FUNCTION__"() "##format"\n", __VA_ARGS__)
#define LogError(format, ...)   shared::logging::log(shared::logging::Level::Error,   __FUNCTION__"() "##format"\n", __VA_ARGS__)
#define LogWarning(format, ...) shared::logging::log(shared::logging::Level::Warning, __FUNCTION__"() "##format"\n", __VA_ARGS__)



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