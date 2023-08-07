#pragma once

#include <ntstatus.h>
#include <ntstrsafe.h>
#include <CustomFeature/object.h>

// Generic string
namespace std
{
    template<typename T>
    struct const_string
    {
    public:
        static const size_t npos = ~static_cast<size_t>(0);
    protected:
        T* _sz_string = nullptr;
        size_t _length = 0;
    public:
        const_string() {};
        ~const_string() {}

        size_t length() const { return _length; }
        const T* c_str() const { return _sz_string; }

        inline bool operator==(const const_string& other) const
        {
            if (_length != other._length)
                return false;
            return RtlCompareMemory(_sz_string, other._sz_string, _length * sizeof(T)) == _length * sizeof(T);
        }
        inline bool operator!=(const const_string& other) const
        {
            if (_length != other._length)
                return true;
            return RtlCompareMemory(_sz_string, other._sz_string, _length * sizeof(T)) != _length * sizeof(T);
        }
        inline T& operator[](int i) const { return _sz_string[i]; }
    };
}

// Basic string
namespace std
{
    template<typename T>
    class basic_string : public const_string<T> {};

    template<>
    class basic_string<wchar_t> : public const_string<wchar_t>, public failable_object<bool>
    {
    public:
        basic_string(const wchar_t* buffer, size_t count)
        {
            if (buffer == nullptr)
            {
                failable_object::_error = true;
                return;
            }
            _length = count + 1;
            _sz_string = new wchar_t[_length];
            if (_sz_string == nullptr)
            {
                failable_object::_error = true;
                return;
            }
            if (RtlStringCchCopyNW(_sz_string, _length, buffer, count) != STATUS_SUCCESS)
            {
                failable_object::_error = true;
                delete _sz_string;
                return;
            }
            _sz_string[count] = 0;
        }
        basic_string(basic_string&& temp)
        {
            _sz_string = temp._sz_string;
            _length = temp._length;
            temp._sz_string = nullptr;
            temp._length = 0;
            temp._error = true;
        }
        ~basic_string()
        {
            if (error() == false)
                delete[] _sz_string;
        }

        basic_string substr(size_t pos, size_t count = npos) const
        {
            if (pos >= _length)
                return basic_string(nullptr, 0);
            if (count == npos)
                count = _length - pos;
            if (pos + count > _length)
                count = _length - pos;
            return basic_string(_sz_string + pos, count);
        }

        size_t find(const wchar_t* sz_string, size_t pos = 0) const
        {
            auto p = wcsstr(_sz_string + pos, sz_string);
            if (p == nullptr)
                return basic_string::npos;
            return p - _sz_string;
        }
    };

    using wstring = basic_string<wchar_t>;
}

namespace std
{
    template<typename T>
    class ref_string: public const_string<T> {};

    template<>
    class ref_string<wchar_t> : public const_string<wchar_t>
    {
    public:
        ref_string(const wchar_t* sz_string)
        {
            _sz_string = const_cast<wchar_t*>(sz_string);
            _length = wcslen(sz_string);
        }
        ~ref_string() {}
    };

    using wstring_ref = ref_string<wchar_t>;
}