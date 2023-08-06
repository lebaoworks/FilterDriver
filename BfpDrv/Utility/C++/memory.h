#pragma once

namespace std {
    // Convert object to rvalue
    template<typename T>
    inline T&& move(T& obj) noexcept { return static_cast<T&&>(obj); }

    // Convert object to rvalue
    template<typename T>
    inline T&& move(T&& obj) noexcept { return static_cast<T&&>(obj); }

    // Swap objects' value
    template<typename T>
    inline void swap(T& a, T& b) noexcept { auto x = move(a); a = move(b); b = move(x); }
    
    // [Warning] Only for single object
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
        inline unique_ptr<T>& operator=(unique_ptr<T>&& uptr) { std::swap(_ptr, uptr._ptr); return *this; }
        unique_ptr<T>& operator=(const unique_ptr<T>& uptr) = delete;

        // Modifiers
        inline T* release() { auto p = _ptr; _ptr = nullptr; return p; }
        inline void reset(T* p) { if (_ptr != nullptr) delete _ptr; _ptr = p; }
        inline void swap(unique_ptr<T>& other) { swap(_ptr, other._ptr); }

        // Observers
        inline T* get() const { return _ptr; }
        inline operator bool() const { return _ptr != nullptr; }

        // Dereference
        inline T& operator*() const { return *_ptr; }
        inline T* operator->() const { return _ptr; }

        // Comparisons
        inline bool operator==(const unique_ptr& other) const { return _ptr == other._ptr; }
        inline bool operator!=(const unique_ptr& other) const { return _ptr != other._ptr; }
        inline bool operator<(const unique_ptr& other) const { return _ptr < other._ptr; }
        inline bool operator>(const unique_ptr& other) const { return _ptr > other._ptr; }
        inline bool operator<=(const unique_ptr& other) const { return _ptr <= other._ptr; }
        inline bool operator>=(const unique_ptr& other) const { return _ptr >= other._ptr; }
    };

    // [Warning] Only for single object
    template<typename T, typename... Args>
    inline unique_ptr<T> make_unique(const Args&... args)
    {
        return unique_ptr<T>(new T(args...));
    }

    template<typename T, typename... Args>
    inline unique_ptr<T> make_unique(Args&&... args)
    {
        return unique_ptr<T>(new T(std::move(args)...));
    }
}