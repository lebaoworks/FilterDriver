#pragma once

#include <C++/memory.h>
#include <CustomFeature/object.h>

namespace std
{
    template<typename T>
    class vector : public failable_object<bool>
    {
    private:
        size_t _capacity;
        T* _array;
        size_t _size;
        
        bool realloc()
        {
            auto _new_cap = (_capacity == 0) ? 8 : _capacity *= 2;
            T* temp = new T[_new_cap];
            if (temp == nullptr)
                return false;
            for (size_t i = 0; i < _size; i++)
                temp[i] = std::move(_array[i]);

            if (_capacity > 0)
                delete[] _array;
            _array = temp;
            _capacity = _new_cap;
            return true;
        }
    public:

        // Constructors
        vector() : _capacity(8), _size(0), _array(new T[_capacity])
        {
            if (_array == nullptr)
                failable_object::_error = true;
        }
        ~vector() { delete[] _array; }

        // Modifiers
        void push_back(const T& element)
        {
            if (_size < _capacity || realloc())
                _array[_size++] = element;
        }
        void push_back(T&& element)
        {
            if (_size < _capacity || realloc())
                _array[_size++] = std::move(element);
        }
        void pop_back() { if (_size > 0) _size--; }
        void erase(size_t index)
        {
            if (index >= _size)
                return;
            for (size_t i = index; i < _size - 1; i++)
                _array[i] = std::move(_array[i+1]);
        }
        void clear()
        {
            _size = 0;
            _capacity = 0;
            delete[]_array;
        }

        // Access elements
        T& operator[](size_t index) { return _array[index]; }
        
        // Capacity
        size_t size() const { return _size; }
        bool empty() const { return _size == 0; }

        // Iterators
        struct iterator
        {
            friend class vector<T>;
        private:
            T* _t;
            iterator(T* t) : _t(t) {}
        public:
            inline iterator& operator++() { _t++; return *this; }
            inline iterator operator++(int) { auto t = *this; _t++; return t; }
            inline bool operator==(const iterator& i) const { return _t == i._t; }
            inline bool operator!=(const iterator& i) const { return _t != i._t; }
            inline T& operator*() const { return *_t; }
        };
        inline iterator begin() const { return iterator(_array); }
        inline iterator end() const { return iterator(_array + _size); }
    };


}