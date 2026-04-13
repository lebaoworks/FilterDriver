#pragma once

#include <utility>

/*********************
*        Defer       *
*********************/

namespace nstd
{
    // Source: https://stackoverflow.com/a/42060129
    // Modified for rvalue
    struct defer_dummy {};
    template<class F>
    struct deferer
    {
        F _f;
        deferer(F&& f) : _f(std::forward<F>(f)) {}
        ~deferer() { _f(); }
    };
    template<class F>
    inline deferer<F> operator*(defer_dummy&&, F&& f)
    {
        return deferer<F>(std::forward<F>(f));
    }
}
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = nstd::defer_dummy{} *[&]()


