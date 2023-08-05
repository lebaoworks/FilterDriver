#pragma once

#include <fltKernel.h>

namespace std
{
    // This is a simple wrapper around the RTL AVL table. It is not intended to be a full implementation of std::map.
    template<typename K, typename V>
    class map
    {
    private:
        struct node { K key; V value; };

        V _dummy;
        RTL_AVL_TABLE _table;
    
        _IRQL_requires_same_
        _Function_class_(_compare)
        static RTL_GENERIC_COMPARE_RESULTS NTAPI _compare(
            _In_ struct _RTL_AVL_TABLE* Table,
            _In_ PVOID first,
            _In_ PVOID second)
        {
            UNREFERENCED_PARAMETER(Table);
            auto& n1 = *reinterpret_cast<node*>(first);
            auto& n2 = *reinterpret_cast<node*>(second);
            if (n1.key < n2.key)
                return GenericLessThan;
            if (n1.key > n2.key)
                return GenericGreaterThan;
            return GenericEqual;
        }

        _IRQL_requires_same_
        _Function_class_(_allocate)
        __drv_allocatesMem(Mem)
        static PVOID NTAPI _allocate(
            _In_ _RTL_AVL_TABLE* Table,
            _In_ CLONG ByteSize)
        {
            UNREFERENCED_PARAMETER(Table);
            return ExAllocatePool2(POOL_FLAG_NON_PAGED, ByteSize, 'pam0');
        }

        _IRQL_requires_same_
        _Function_class_(_free)
        static void NTAPI _free(
            _In_ _RTL_AVL_TABLE* Table,
            _In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Buffer)
        {
            UNREFERENCED_PARAMETER(Table);
            ExFreePool(Buffer);
        }
    public:

        // Constructor
        map() { RtlInitializeGenericTableAvl(&_table, _compare, _allocate, _free, nullptr); }
        ~map() { clear(); }

        // Modifiers
        bool insert(const K& key, const V& value)
        {
            node n = { key, value };
            return RtlInsertElementGenericTableAvl(&_table, (PVOID)&n, sizeof(node), nullptr) != nullptr;
        }
        void erase(const K& key) { RtlDeleteElementGenericTableAvl(&_table, (PVOID)&key); }
        void clear()
        {
            for (auto p = RtlEnumerateGenericTableAvl(&_table, TRUE); p != NULL; p = RtlEnumerateGenericTableAvl(&_table, FALSE))
            {
                Log("Delete %p", p);
                RtlDeleteElementGenericTableAvl(&_table, p);
            }
        }

        // Capacity
        bool empty() const { return RtlNumberGenericTableElementsAvl(&_table) == 0; }
        size_t size() const { return RtlNumberGenericTableElementsAvl(&_table); }

        // Lookup
        V* find(const K& key) const
        {
            auto p = reinterpret_cast<node*>(RtlLookupElementGenericTableAvl(const_cast<PRTL_AVL_TABLE>(&_table), (PVOID)&key));
            if (p == nullptr)
                return nullptr;
            return reinterpret_cast<V*>(&p->value);
        }

        // Element access
        //V& operator [](const K& key)
        //{
        //    //node n{ key };
        //    BOOLEAN added = FALSE;
        //    auto p = RtlInsertElementGenericTableAvl(&_table, reinterpret_cast<PVOID>(&const_cast<K&>(key)), sizeof(K), &added);
        //    if (p != nullptr)
        //    {
        //        if (added)
        //            Log("Added %p", p);
        //        return reinterpret_cast<node*>(p)->value;
        //    }
        //    Log("Alloc failed");
        //    return _dummy;
        //}
       
        //void for_each(std::function<void(const KeyT&, const ValueT&)> func)
        //{
        //    PVOID p;
        //    KeyT key;
        //    while (nullptr != (p = RtlEnumerateGenericTableAvl(&_table, TRUE, &p, &key)))
        //    {
        //        func(key, *(ValueT*)p);
        //    }
        //}
    };

}
