#pragma once

namespace std
{
    // Double linked list
    template<typename T>
    class linked_list
    {
    private:
        // Double-linked node
        struct node
        {
            T _data;
            node* _prev = nullptr;
            node* _next = nullptr;
            node(const T& data) noexcept : _data(data) {}
            node(T&& data) noexcept : _data(std::move(data)) {}
            void link_next(node& n) noexcept
            {
                if (_next == nullptr)
                {
                    _next = &n;
                    n._prev = this;
                }
                else
                {
                    auto& node_next = *_next;
                    _next = &n;
                    n._prev = this;
                    n._next = &node_next;
                    node_next._prev = &n;
                }
            }
            void link_prev(node& n) noexcept
            {
                if (_prev == nullptr)
                {
                    _prev = &n;
                    n._next = this;
                }
                else
                {
                    auto& node_prev = *_prev;
                    _prev = &n;
                    n._next = this;
                    n._prev = &node_prev;
                    node_prev._next = &n;
                }
            }
        };
        size_t _size = 0;
        node* _front = nullptr;
        node* _rear = nullptr;
    public:
        // Constructors
        linked_list() {}
        linked_list(const linked_list<T>&) = delete;
        linked_list(linked_list<T>&& q) = delete;//: _size(q._size), _pivot(std::move(q._pivot) { q._size = 0; q._front = q._rear = nullptr; }
        ~linked_list() { clear(); }

        // Assignments
        linked_list<T>& operator=(const linked_list<T>&) = delete;
        linked_list<T>& operator=(linked_list<T>&& q) = delete;

        // Element access
        inline T& front() const { return _front->_data; }
        inline T& back() const { return _rear->_data; }

        // Capacity
        inline bool empty() const { return _front == nullptr; }
        inline size_t size() const { return _size; }

        // Iterators
        struct iterator
        {
            friend class linked_list;
        private:
            node* _node;
            iterator(node* n) noexcept : _node(n) {}
        public:
            inline iterator& operator++() { _node = _node->_next; return *this; }
            inline iterator operator++(int) { auto t = *this; _node = _node->_next; return t; }
            inline bool operator==(const iterator& i) const { return _node == i._node; }
            inline bool operator!=(const iterator& i) const { return _node != i._node; }
            inline T& operator*() const { return _node->_data; }
        };
        inline iterator begin() const { return iterator(_front); }
        inline iterator end() const { return iterator(nullptr); }

        // Modifiers
        inline void clear()
        {
            while (!empty())
                erase(begin());
        }
        inline iterator insert(const iterator& i, const T& t)
        {
            auto n = new node(t);
            if (n == nullptr)
                return end();
            if (empty())
            {
                _front = _rear = n;
                _size = 1;
                return iterator(n);
            }
            if (i == end())
            {
                _rear->link_next(*n);
                _rear = n;
                _size++;
                return iterator(n);
            }
            else
            {
                i._node->link_prev(*n);
                if (i._node == _front)
                    _front = n;
                _size++;
                return iterator(n);
            }
        }
        inline iterator insert(const iterator& i, T&& t)
        {
            auto n = new node(std::move(t));
            if (n == nullptr)
                return end();
            if (empty())
            {
                _front = _rear = n;
                _size = 1;
                return iterator(n);
            }
            if (i == end())
            {
                _rear->link_next(*n);
                _rear = n;
                _size++;
                return iterator(n);
            }
            else
            {
                i._node->link_prev(*n);
                if (i._node == _front)
                    _front = n;
                _size++;
                return iterator(n);
            }
        }
        inline iterator erase(const iterator& i)
        {
            if (empty() || i == end())
                return end();
            if (i == begin())
            {
                if (_front == _rear)
                {
                    delete _front;
                    _front = _rear = nullptr;
                    _size = 0;
                    return end();
                }
                else
                {
                    auto node_next = _front->_next;
                    delete _front;
                    node_next->_prev = nullptr;
                    _front = node_next;
                    _size--;
                    return iterator(_front);
                }
            }
            else
            {
                auto node_prev = i._node->_prev;
                auto node_next = i._node->_next;
                delete i._node;
                node_prev->_next = node_next;
                if (node_next != nullptr)
                {
                    node_next->_prev = node_prev;
                    return iterator(node_next);
                }
                else
                {
                    _rear = node_prev;
                    return end();
                }
            }
        }
    };

    template<typename T>
    class list : public linked_list<T>
    {
    public:
        using linked_list<T>::iterator;

        // Constructors
        list() {}
        list(const list<T>&) = delete;
        list(list<T>&& q) = delete;
        ~list() {}

        // Assignments
        list<T>& operator=(const list<T>&) = delete;
        list<T>& operator=(list<T>&& q) = delete;

        // Modifiers
        inline iterator push_front(const T& t) { return linked_list<T>::insert(linked_list<T>::begin(), t); }
        inline iterator push_front(T&& t) { return linked_list<T>::insert(linked_list<T>::begin(), std::move(t)); }
        inline iterator push_back(const T& t) { return linked_list<T>::insert(linked_list<T>::end(), t); }
        inline iterator push_back(T&& t) { return linked_list<T>::insert(linked_list<T>::end(), std::move(t)); }
        inline void pop_front() { linked_list<T>::erase(linked_list<T>::begin()); }
        inline void pop_back() { linked_list<T>::erase(--linked_list<T>::end()); }
    };
}