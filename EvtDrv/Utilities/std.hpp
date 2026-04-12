#pragma once

/*********************
*       Traits       *
*********************/

namespace std
{
    struct true_type
    {
        static constexpr bool value = true;
        using value_type = bool;
        using type = true_type;
        constexpr operator value_type() const noexcept { return value; }
    };

    struct false_type
    {
        static constexpr bool value = false;
        using value_type = bool;
        using type = false_type;
        constexpr operator value_type() const noexcept { return value; }
    };

    template <typename T, T v>
    struct integral_constant
    {
        static constexpr T value = v; // Static member to hold the value

        using value_type = T; // Type alias for the value type
        using type = integral_constant; // Type alias for the type itself

        constexpr operator value_type() const noexcept { return value; }

        constexpr value_type operator()() const noexcept { return value; }
    };


    template <typename T, typename U>
    struct is_same : false_type {};

    template <typename T>
    struct is_same<T, T> : true_type {};


    template<typename T>
    struct is_lvalue_reference : false_type {};

    template<typename T>
    struct is_lvalue_reference<T&> : true_type {};


    template <typename T>
    struct is_class {
    private:
        // Helper function to detect class type
        template <typename U>
        static char test(int U::*);

        // Fallback function
        template <typename U>
        static int test(...);

    public:
        // If the first overload is chosen, T is a class
        static constexpr bool value = sizeof(test<T>(0)) == sizeof(char);
    };


    template<typename Base, typename Derived>
    class is_base_of
    {
    private:
        static constexpr bool Test(Base*) noexcept { return true; };
        static constexpr bool Test(...) noexcept { return false; };
    public:
        static constexpr bool value = Test(static_cast<Derived*>(nullptr));
    };


    template<typename T>
    struct is_array : false_type {};

    template<typename T>
    struct is_array<T[]> : true_type {};

    template<typename T, size_t N>
    struct is_array<T[N]> : true_type {};
}


/*********************
*       Utility      *
*********************/

namespace std
{
    // Handle lvalue
    template<typename T>
    inline T&& move(T& obj) noexcept { return static_cast<T&&>(obj); }

    // Handle rvalue
    template<typename T>
    inline T&& move(T&& obj) noexcept { return static_cast<T&&>(obj); }


    template<typename T>
    struct remove_reference { typedef T type; };

    template<typename T>
    struct remove_reference<T&> { typedef T type; };

    template<typename T>
    struct remove_reference<T&&> { typedef T type; };

    template<typename T>
    inline T&& forward(typename remove_reference<T>::type& t) noexcept { return static_cast<T&&>(t); }

    template<typename T>
    inline T&& forward(typename remove_reference<T>::type&& t) noexcept
    {
        static_assert(is_lvalue_reference<T>::value == false, "bad forward call");
        return static_cast<T&&>(t);
    }
}



