#ifdef _TEST
#include <base.h>

namespace base
{
    struct test_dummy
    {
        int x;
        test_dummy(int v) { x = v; }
    };

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS test()
    {
        // Test new
        {
            Log("[*] new");
            auto x = new int(5);
            if (x == nullptr)
                return STATUS_NO_MEMORY;
            delete x;
        }

        // Test placement new
        {
            test_dummy x(5);
            new (&x) test_dummy(10);
            if (x.x != 10)
            {
                LogError("[!] replacement new fail");
                return STATUS_UNSUCCESSFUL;
            }
            (&x)->~test_dummy();
        }

        // Test traits
        {
            ASSERT_EQ((bool)base::is_array<int>(), false, "is_array<int> fail");
            ASSERT_EQ((bool)base::is_array<int[]>(), true, "is_array<int[]> fail");
            ASSERT_EQ((bool)base::is_array<int[5]>(), true, "is_array<int[5]> fail");

            ASSERT_EQ(bool(base::is_base_of<failable, failable>()), true, "is_base_of<failable, failable> fail");
            ASSERT_EQ(bool(base::is_base_of<int, double>()), false, "is_base_of<int, double> fail");
        }

        // Test make
        {
            auto result = base::make<int>(5);
            if (result.error() != STATUS_SUCCESS)
            {
                LogError("[!] make fail: %X", result.error());
                return result.error();
            }
            int* x = result.release();
            defer { delete x; };
            if (*x != 5)
            {
                LogError("[!] make unexpected value");
                return STATUS_UNSUCCESSFUL;
            }
        }
        return STATUS_SUCCESS;
    }
}

#endif