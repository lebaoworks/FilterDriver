#pragma once

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

    template<typename T>
    class unique_ptr
    {
    private:
        T* _ptr = nullptr;
    public:
        // Constructors
        unique_ptr() {}
        unique_ptr(T* p) : _ptr(p) {}
        unique_ptr(const unique_ptr<T>& uptr) = delete;
        unique_ptr(unique_ptr<T>&& uptr) : _ptr(uptr._ptr) { uptr._ptr = nullptr; }
        ~unique_ptr() { if (_ptr != nullptr) delete _ptr; }
        
        // Assignments
        unique_ptr<T>& operator=(unique_ptr<T>&& uptr) { swap(_ptr, uptr._ptr); return *this; }
        unique_ptr<T>& operator=(const unique_ptr<T>& uptr) = delete;

        // Modifiers
        T* release() { auto p = _ptr; _ptr = nullptr; return p; }
        void reset(T* p) { if (_ptr != nullptr) delete _ptr; _ptr = p; }
        void swap(unique_ptr<T>& other) { swap(_ptr, other._ptr); }

        // Observers
        T* get() const { return _ptr; }
        operator bool() const { return _ptr != nullptr; }

        // Deference
        T& operator*() const { return *_ptr; }
        T* operator->() const { return _ptr; }
    };
    template<typename T, typename... Args>
    unique_ptr<T> make_unique(Args... args)
    {
        return unique_ptr<T>(new T(args...));
    }
}