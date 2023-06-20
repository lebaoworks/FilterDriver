#pragma once

namespace std
{
    template<typename T>
    class queue
    {
    private:
        struct node
        {
            T data;
            node* next;
        };
        size_t _size = 0;
        node* _front = nullptr;
        node* _rear = nullptr;
    public:
        // Constructors
        queue() {}
        queue(const queue<T>&) = delete;
        queue(queue<T>&& q) : _size(q._size), _front(q._front), _rear(q._rear) { q._size = 0; q._front = q._rear = nullptr; }
        ~queue() { while (!empty()) pop(); }

        // Assignments
        queue<T>& operator=(const queue<T>&) = delete;
        queue<T>& operator=(queue<T>&& q) { _size = q._size; _front = q._front; _rear = q._rear; q._size = 0; q._front = q._rear = nullptr; return *this; }

        // Modifiers
        void push(const T& data)
        {
            auto n = new node{ data, nullptr };
            if (n == nullptr)
                return;
            _size++;
            if (_front == nullptr)
                _front = _rear = n;
            else
            {
                _rear->next = n;
                _rear = n;
            }
        }
        void pop()
        {
            if (_front == nullptr)
                return;
            _size--;
            auto n = _front;
            _front = _front->next;
            delete n;
        }

        // Observers
        inline bool empty() const { return _front == nullptr; }
        inline T& front() const { return _front->data; }
        inline T& back() const { return _rear->data; }
        inline size_t size() const { return _size; }
    };
}