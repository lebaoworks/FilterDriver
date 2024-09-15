#include "krn.hpp"

#ifdef  _TEST

#pragma warning(disable: 4127)
#define ASSERTION_BUGCHECK_CODE 0xDEADDEAD
#define FAIL KeBugCheckEx(ASSERTION_BUGCHECK_CODE, (ULONG_PTR)__FILE__, __LINE__, 0, 0)
#ifdef ASSERT
    #undef ASSERT
#endif
#define ASSERT(condition) do { if ((condition) == false) { KeBugCheckEx(ASSERTION_BUGCHECK_CODE, (ULONG_PTR)__FILE__, __LINE__, 0, 0); } } while (0)

static void test_expected()
{
    auto f1 = [](int x) -> krn::expected<int> { if (x == 0) return krn::unexpected(STATUS_INVALID_PARAMETER); else return 5; };
    auto a = f1(5);
    ASSERT(a.status() == STATUS_SUCCESS);
    ASSERT(a.value() == 5);
    auto b = f1(0);
    ASSERT(b.status() == STATUS_INVALID_PARAMETER);


    struct A
    {
        int _x;
        A(int x) : _x(x) {}
    };
    auto f2 = [](int x) -> krn::expected<A> { if (x == 0) return krn::unexpected(STATUS_INVALID_PARAMETER); else return A(x); };
    auto c = f2(5);
    ASSERT(c.status() == STATUS_SUCCESS);
    ASSERT(c.value()._x == 5);
    auto d = f2(0);
    ASSERT(d.status() == STATUS_INVALID_PARAMETER);
}

void krn::test()
{
    test_expected();
}

#endif
