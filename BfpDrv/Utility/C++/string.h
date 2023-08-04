#pragma once

#include <ntstatus.h>
#include <ntstrsafe.h>
#include <CustomFeature/object.h>

namespace std
{
    template<typename T> 
    struct stringbuf : public failable_object<bool>
    {
    public:
        static const size_t npos = ~static_cast<size_t>(0);
    protected:
        T* _sz_string = nullptr;
        size_t _length = 0;
    public:
        stringbuf(const T* buffer, size_t count)
        {
            _length = count + 1;
            _sz_string = new T[_length];
            if (_sz_string == nullptr)
            {
                failable_object::_error = true;
                return;
            }
            RtlCopyMemory(_sz_string, buffer, count * sizeof(T));
            _sz_string[count] = 0;
        }
        ~stringbuf()
        {
            if (error() == false)
                delete[] _sz_string;
        }

        size_t length() const { return _length; }
        const T* c_str() const { return _sz_string; }

        inline bool operator==(const stringbuf& i) const { return wcscmp(_sz_string, i._sz_string) == 0; }
        inline bool operator!=(const stringbuf& i) const { return wcscmp(_sz_string, i._sz_string) != 0; }
        inline T& operator[](int i) const { return _sz_string[i]; }
    };

    template<typename T>
    class base_string : public stringbuf<T> {};
    
    template<>
    class base_string<wchar_t> : public stringbuf<wchar_t>
    {
    public:
        using stringbuf<wchar_t>::stringbuf;

        size_t find(const wchar_t* sz_string, size_t pos = 0) const
        {
            auto p = wcsstr(_sz_string + pos, sz_string);
            if (p == nullptr)
                return stringbuf<wchar_t>::npos;
            return p - _sz_string;
        }
    };

    using wstring = base_string<wchar_t>;
}