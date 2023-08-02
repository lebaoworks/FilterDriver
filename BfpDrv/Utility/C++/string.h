#pragma once

#include <ntstrsafe.h>
#include <CustomFeature/object.h>

namespace std
{
    template<typename T>
    class base_string : public failable_object<bool>
    {
    };

    template<>
    class base_string<wchar_t> : public failable_object<bool>
    {
    public:
        static const size_t npos = ~static_cast<size_t>(0);
    private:
        wchar_t* _sz_string = nullptr;
        size_t _length = 0;
    public:
        base_string(const wchar_t* buffer, size_t count)
        {
            _sz_string = new wchar_t[count + 1];
            if (_sz_string == nullptr)
            {
                failable_object::_error = true;
                return;
            }
            RtlStringCchCopyNW(
                _sz_string, count + 1,
                buffer, count
            );
            _length = wcslen(_sz_string);
        }
        ~base_string()
        {
            if (error() == false)
                delete[] _sz_string;
        }

        size_t length() const { return _length; }
        wchar_t* c_str() const { return _sz_string; }

        size_t find(const wchar_t* sz_string, size_t pos = 0) const {
            auto p = wcsstr(_sz_string + pos, sz_string);
            if (p == nullptr)
                return npos;
            return p - _sz_string;
        }

        inline bool operator==(const base_string& i) const { return wcscmp(_sz_string, i._sz_string) == 0; }
        inline bool operator!=(const base_string& i) const { return wcscmp(_sz_string, i._sz_string) != 0; }
        inline wchar_t& operator[](int i) const { return _sz_string[i]; }
    };

    using wstring = base_string<wchar_t>;
}