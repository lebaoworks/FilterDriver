#pragma once
#include <sal.h>

/*********************
*       Logging      *
*********************/

namespace shared {
    void log(_In_ unsigned int level, _In_z_ char* szString);

    template<typename... T>
    void log(_In_ unsigned int level, _In_z_ char* format, T... args)
    {
        log(level, format);
    }
}

#define Log(...) shared::log(1, __VA_ARGS__)

/*********************
*         C++        *
*********************/

namespace std {
    // Convert object to rvalue
    template<typename T>
    inline T&& move(T& obj) { return static_cast<T&&>(obj); }

    // Convert object to rvalue
    template<typename T>
    inline T&& move(T&& obj) { return static_cast<T&&>(obj); }

    // Swap objects' value
    template<typename T>
    inline void swap(T& a, T& b) { auto x = move(a); a = move(b); b = move(x); }
}

// Source: https://stackoverflow.com/a/42060129
// Modified for rvalue
#ifndef defer
struct defer_dummy {};
template<class F>
struct deferrer
{
    F _f;
    deferrer(F&& f) _f(f) {}
    ~deferrer() { _f(); }
};
template<class F>
inline deferrer<F> operator*(defer_dummy, F&& f)
{
    return deferer<F>(std::move(f));
}
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#endif // defer