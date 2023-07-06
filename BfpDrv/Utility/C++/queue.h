#pragma once

namespace std
{
    template<typename T>
    class queue
    {
    private:
        struct node
        {
            T _data;
            node* _next = nullptr;
            node(const T& data) : _data(data) {}
            node(T&& data) : _data(std::move(data)) {}
        };
        size_t _size = 0;
        node* _front = nullptr;
        node* _rear = nullptr;

        void _push(_In_ node* n)
        {
            if (n == nullptr)
                return;
            _size++;
            if (_front == nullptr)
                _front = _rear = n;
            else
            {
                _rear->_next = n;
                _rear = n;
            }
        }
        void _pop()
        {
            if (_front == nullptr)
                return;
            _size--;
            auto n = _front;
            _front = _front->_next;
            delete n;
        }
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
        inline void push(const T& data) { _push(new node(data)); }
        inline void push(T&& data) { _push(new node(move(data))); }
        inline void pop() { return _pop(); }

        // Observers
        inline bool empty() const { return _front == nullptr; }
        inline T& front() const { return _front->_data; }
        inline T& back() const { return _rear->_data; }
        inline size_t size() const { return _size; }

        // Iterators
        struct iterator
        {
            friend class queue<T>;
        private:
            node* n;
            iterator(node* n) : n(n) {}
        public:
            inline iterator& operator++() { n = n->_next; return *this; }
            inline iterator operator++(int) { auto t = *this; n = n->_next; return t; }
            inline bool operator==(const iterator& i) const { return n == i.n; }
            inline bool operator!=(const iterator& i) const { return n != i.n; }
            inline T& operator*() const { return n->_data; }
        };
        inline iterator begin() const { return iterator(_front); }
        inline iterator end() const { return iterator(nullptr); }
    };
}