#pragma once

namespace std
{
    template<typename T>
    class lock_guard
    {
    private:
        T& _mutex;
    public:
        lock_guard(T& Mutex) : _mutex(Mutex) { _mutex.lock(); }
        ~lock_guard() { _mutex.unlock(); }
    };
}