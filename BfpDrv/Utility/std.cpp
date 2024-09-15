#include <std.hpp>

_When_(return != nullptr, __drv_allocatesMem(Mem))
void* _cdecl operator new(_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, '++C0'); }

_Ret_notnull_
void* _cdecl operator new(_In_ size_t size, _Inout_updates_(size) void* buffer) { size; return buffer;}

void _cdecl operator delete(_Pre_notnull_ __drv_freesMem(Mem) void* object) { ExFreePool(object); }

void _cdecl operator delete(_Inout_updates_(size) void* object, _In_ size_t size) { size; ExFreePool(object); }

#ifdef _TEST

#pragma warning(disable: 4127)
#define ASSERTION_BUGCHECK_CODE 0xDEADDEAD
#define FAIL KeBugCheckEx(ASSERTION_BUGCHECK_CODE, (ULONG_PTR)__FILE__, __LINE__, 0, 0)
#ifdef ASSERT
    #undef ASSERT
#endif
#define ASSERT(condition) do { if ((condition) == false) { KeBugCheckEx(ASSERTION_BUGCHECK_CODE, (ULONG_PTR)__FILE__, __LINE__, 0, 0); } } while (0)

static void test_alloc()
{
    // new
    auto x = new int(5);
    ASSERT(x != nullptr);
    delete x;

    // replacement new
    int y(5);
    new (&y) int(10);
    ASSERT(y == 10);
}

static void test_traits()
{
    ASSERT(std::true_type::value == true);
    ASSERT(std::false_type::value == false);

    auto integral = std::integral_constant<int, 5>::value;
    ASSERT(integral == 5);

    bool same = std::is_same<int, int>::value;
    bool not_same = std::is_same<int, double>::value;
    ASSERT(same == true);
    ASSERT(not_same == false);

    ASSERT(std::is_lvalue_reference<int>::value == false);
    ASSERT(std::is_lvalue_reference<int&>::value == true);

    struct A {};
    class B {};
    enum class E {};
    union U { class UC {}; };
    ASSERT(std::is_class<A>::value == true);
    ASSERT(std::is_class<B>::value == true);
    ASSERT(std::is_class<B*>::value == false);
    ASSERT(std::is_class<B&>::value == false);
    ASSERT(std::is_class<const B>::value == true);
    ASSERT(std::is_class<E>::value == false);
    ASSERT(std::is_class<int>::value == false);
    ASSERT(std::is_class<struct S>::value == true);
    ASSERT(std::is_class<class C>::value == true);

    ASSERT(std::is_array<int>::value == false);
    ASSERT(std::is_array<int[]>::value == true);
    ASSERT(std::is_array<int[5]>::value == true);

    struct base {};
    struct derived : base {};
    bool is_base_of_1 = std::is_base_of<base, derived>::value;
    ASSERT(is_base_of_1 ==  true);
    bool is_base_of_2 = std::is_base_of<int, double>::value;
    ASSERT(is_base_of_2 == false);
}

static void test_utility()
{
    // move
    {
        struct A
        {
            int _x;
            A(int x) : _x(x) {};
            A(const A&) = delete;
            A(A&& a) : _x(a._x) { a._x = 0; }
        };

        A a(5);
        A b(std::move(a));

        ASSERT(a._x == 0); // move
        ASSERT(b._x == 5); // move
    }
    // forward
    {
        struct A
        {
            int _x;
            A(int x) : _x(x) {};
            A(const A& a) : _x(a._x) {};
            A(A&& a) : _x(a._x + 1) { a._x = 0; }
        };

        A a(5);
        A b(std::forward<A&>(a));
        A c(std::forward<A&&>(A(10)));

        ASSERT(a._x == 5); // keep
        ASSERT(b._x == 5); // copy
        ASSERT(c._x == 11); // move
    }
    // swap
    {
        int a = 5;
        int b = 10;
        std::swap(a, b);

        ASSERT(a == 10);
        ASSERT(b == 5);
    }
}

static void test_memory()
{
    // default_delete
    {
        struct A
        {
            int& _x;
            A(int& x) : _x(x) {}
            ~A() { --_x; }
        };

        int x = 5;
        auto a = new A(x);
        ASSERT(a != nullptr);

        std::default_delete<A> del;
        del(a);
        ASSERT(x == 4);
    }
    // unique_ptr
    {
        // default constructor
        {
            std::unique_ptr<int> x;
            ASSERT(x.get() == nullptr);
        }

        // move constructor
        {
            std::unique_ptr<int> x(new int(5));
            ASSERT(x.get() != nullptr);
            ASSERT(*x == 5);

            std::unique_ptr<int> y(std::move(x));
            ASSERT(x.get() == nullptr);
            ASSERT(y.get() != nullptr);
            ASSERT(*y == 5);
        }

        // move assignment
        {
            std::unique_ptr<int> x(new int(5));
            ASSERT(x.get() != nullptr);
            ASSERT(*x == 5);

            std::unique_ptr<int> y;
            y = std::move(x);
            ASSERT(x.get() == nullptr);
            ASSERT(y.get() != nullptr);
            ASSERT(*y == 5);
        }
           
        // reset
        {
            std::unique_ptr<int> x(new int(5));
            ASSERT(x.get() != nullptr);
            ASSERT(*x == 5);

            x.reset(new int(10));
            ASSERT(x.get() != nullptr);
            ASSERT(*x == 10);
        }

        // operator bool
        {
            std::unique_ptr<int> x;
            ASSERT(!x);

            std::unique_ptr<int> y(new int(5));
            ASSERT(y);
        }

        // operator*
        {
            std::unique_ptr<int> x(new int(5));
            ASSERT(*x == 5);
        }

        // operator->
        {
            struct A
            {
                int _x;
                A(int x) : _x(x) {}
            };

            std::unique_ptr<A> x(new A(5));
            ASSERT(x->_x == 5);
        }

        // operator==, operator!=
        {
            auto a = new int(5);
            ASSERT(a != nullptr);
            auto b = new int(5);
            ASSERT(b != nullptr);

            std::unique_ptr<int, void(*)(int*)> x(a, [](int*) {});
            std::unique_ptr<int, void(*)(int*)> y(a, [](int*) {});
            std::unique_ptr<int, void(*)(int*)> z(b, [](int*) {});

            ASSERT(x == y);
            ASSERT(x != z);

            delete a;
            delete b;
        }
    }
}

static void test_defer()
{
    int x = 5;
    {
        defer { x--; };
        x++;
        ASSERT(x == 6);
    }
    ASSERT(x == 5);

}

void std::test()
{
    test_alloc();
    test_traits();
    test_utility();
    test_memory();
    test_defer();
}

#endif