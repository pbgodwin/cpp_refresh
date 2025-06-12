#pragma once

template <typename T, size_t N>
class SmallVector {
    union VectorStore {
        T stack[N];
        T* heap_data_ptr = nullptr;

        VectorStore() {}
        ~VectorStore() {}
    };

    public:
        SmallVector<T, N>() : m_size(0), m_capacity(N), m_on_stack(true) {}
        SmallVector<T, N>(size_t size) : SmallVector<T, N>(size, 0) { }

        SmallVector<T, N>(size_t size, T default_value) : 
                            m_size(size), 
                            m_capacity(m_size > N ? m_size + N : N), 
                            m_on_stack(m_size <= N) {
            if (m_on_stack) {
                for (int i = 0; i < m_size; i++) {
                    m_storage.stack[i] = T(default_value);
                }
            } else {
                m_storage.heap_data_ptr = new T[m_capacity] { default_value };
            }
        }

        SmallVector<T, N>(std::initializer_list<T> init) : 
                        m_size(init.size()), 
                        m_capacity(m_size > N ? m_size : N),
                        m_on_stack(m_size <= N) {
            T* to_modify;
            if (m_on_stack) {
                to_modify = m_storage.stack;
            } else {
                m_storage.heap_data_ptr = reinterpret_cast<T*>(new std::byte[m_capacity * sizeof(T)]);
                to_modify = m_storage.heap_data_ptr;
            }

            int i = 0;
            for (auto v : init) {
                new (to_modify + i) T(v);
                ++i;
            }
        }

        SmallVector<T, N>(SmallVector<T,N>&& src) noexcept {
            if (src.m_on_stack) {
                for (size_t i = 0; i < src.m_size; ++i) {
                    new (m_storage.stack + i) T(std::move(src.m_storage.stack[i]));
                }
            } else {
                m_storage.heap_data_ptr = reinterpret_cast<T*>(new std::byte[src.m_capacity * sizeof(T)]);
                std::swap(m_storage.heap_data_ptr, src.m_storage.heap_data_ptr);
            }

            m_size = src.m_size;
            m_capacity = src.m_capacity;
            m_on_stack = src.m_on_stack;

            src.m_size = 0;
            src.m_on_stack = true;
        }

        SmallVector<T, N>& operator=(SmallVector<T,N>&& src) noexcept {
            if (!empty()) {
                for (const T* it = begin(); it != end(); ++it) {
                    it->~T();
                }
            }
            
            if (src.m_on_stack) {
                for (size_t i = 0; i < src.m_size; ++i) {
                    new (m_storage.stack + i) T(std::move(src.m_storage.stack[i]));
                }
            } else {
                m_storage.heap_data_ptr = reinterpret_cast<T*>(new std::byte[src.m_capacity * sizeof(T)]);
                std::swap(m_storage.heap_data_ptr, src.m_storage.heap_data_ptr);
            }

            m_size = src.m_size;
            m_capacity = src.m_capacity;
            m_on_stack = src.m_on_stack;

            src.m_size = 0;
            src.m_on_stack = true;

            return *this;
        }

        SmallVector<T, N>(const SmallVector<T, N>& copy) {
            if (copy.m_on_stack) {
                for (size_t i = 0; i < copy.m_size; ++i) {
                    new (m_storage.stack + i) T(copy.m_storage.stack[i]);
                }
            } else {
                m_storage.heap_data_ptr = reinterpret_cast<T*>(new std::byte[copy.m_capacity * sizeof(T)]);
                for (int i = 0; i < copy.m_size; i++) {
                    new (m_storage.heap_data_ptr + i) T(copy[i]);
                }
            }

            m_size = copy.m_size;
            m_capacity = copy.m_capacity;
            m_on_stack = copy.m_on_stack;
        }

        ~SmallVector<T, N>() {
            for (const T* it = begin(); it != end(); ++it) {
                it->~T();
            }

            if (!m_on_stack) {
                delete[] reinterpret_cast<std::byte*>(m_storage.heap_data_ptr);
            }

            m_storage.heap_data_ptr = nullptr;
            m_size = 0;
            m_capacity = N;
            m_on_stack = true;
        }

        T& operator[](size_t index) {
            return get_element(index);
        }

        const T& operator[](size_t index) const {
            return get_element(index);
        }

        SmallVector<T, N>& operator=(const SmallVector<T, N>& copy) {
            if (this == 0) {
                return *this;
            }

            if (!empty()) {
                for (const T* it = begin(); it != end(); ++it) {
                    it->~T();
                }
            }

            if (!m_on_stack) {
                delete[] reinterpret_cast<std::byte*>(m_storage.heap_data_ptr);
            }

            if (copy.m_on_stack) {
                for (size_t i = 0; i < copy.m_size; ++i) {
                    new (m_storage.stack + i) T(copy.m_storage.stack[i]);
                }            
            } else {
                m_storage.heap_data_ptr = reinterpret_cast<T*>(new std::byte[copy.m_capacity * sizeof(T)]);
                for (int i = 0; i < copy.m_size; i++) {
                    new (m_storage.heap_data_ptr + i) T(copy[i]);
                }
            }

            m_size = copy.m_size;
            m_capacity = copy.m_capacity;
            m_on_stack = copy.m_on_stack;
            return *this;
        }

        T* begin() { 
            if (m_on_stack) {
                return &(m_storage.stack[0]);
            } else {
                return m_storage.heap_data_ptr;
            }
        }

        const T* begin() const { 
            if (m_on_stack) {
                return &(m_storage.stack[0]);
            } else {
                return m_storage.heap_data_ptr;
            }
        }

        T* end() { 
            if (m_on_stack) {
                return &(m_storage.stack[m_size]);
            } else {
                return (m_storage.heap_data_ptr + m_size);
            }
        }

        const T* end() const { 
            if (m_on_stack) {
                return &(m_storage.stack[m_size]);
            } else {
                return (m_storage.heap_data_ptr + m_size);
            }
        }

    private:
        size_t m_size;
        size_t m_capacity;
        bool m_on_stack;
        VectorStore m_storage;

        T& get_element(size_t index) {
            if (m_on_stack) {
                return m_storage.stack[index];
            } else {
                return m_storage.heap_data_ptr[index];
            }
        }

        const T& get_element(size_t index) const {
            if (m_on_stack) {
                return m_storage.stack[index];
            } else {
                return m_storage.heap_data_ptr[index];
            }
        }

    public:
        T at(size_t index) {             
            if (index >= m_size) {
                throw std::out_of_range("index out of range of vector");
            }
            return get_element(index); 
        }

        const T at(size_t index) const {
            if (index >= m_size) {
                throw std::out_of_range("index out of range of vector");
            }
            return get_element(index); 
        }

        template<class U = T>
        void push_back(U&& element) {
            if (m_on_stack) {
                if (m_size < N) { // still on the stack
                    // construct the stack element in the buffer
                    new (m_storage.stack + m_size) T(std::forward<U>(element));
                } else { // if we've exhausted stack space, it's time to grow onto the heap
                    size_t new_capacity = (N == 0) ? 1 : N * 2;
                    
                    // allocate raw memory for the new capacity
                    T* new_heap_buffer = reinterpret_cast<T*>(new std::byte[new_capacity * sizeof(T)]);

                    // 2. Move elements from the stack to the new heap buffer
                    for (size_t i = 0; i < m_size; ++i) {
                        new (new_heap_buffer + i) T(std::move_if_noexcept(m_storage.stack[i]));
                        (m_storage.stack + i)->~T();
                    }
                    
                    // finally, place the element
                    new (new_heap_buffer + m_size) T(element);

                    // Update internal state 
                    m_storage.heap_data_ptr = new_heap_buffer;
                    m_on_stack = false;
                    m_capacity = new_capacity;
                }
            } else { // already on heap
                if (m_size < m_capacity) {
                    new (m_storage.heap_data_ptr + m_size) T(element);
                } else { // Heap is full, grow the space
                     size_t new_capacity = (m_capacity == 0) ? 1 : m_capacity * 2;
                    
                    // allocate raw memory for the new capacity
                    T* new_heap_buffer = reinterpret_cast<T*>(new std::byte[new_capacity * sizeof(T)]);

                    // 2. Move elements from the stack to the new heap buffer
                    for (size_t i = 0; i < m_size; ++i) {
                        new (new_heap_buffer + i) T(std::move_if_noexcept(m_storage.heap_data_ptr[i]));
                        (m_storage.heap_data_ptr + i)->~T();
                    }
                    
                    // finally, place the element
                    new (new_heap_buffer + m_size) T(element);

                    // Update internal state
                    delete[] reinterpret_cast<std::byte*>(m_storage.heap_data_ptr);
                    m_storage.heap_data_ptr = new_heap_buffer;
                    m_on_stack = false;
                    m_capacity = new_capacity;
                }
            }
            m_size++;
        }

        void pop_back() {
            if (m_size == 0) { 
                throw std::out_of_range("vector already empty");
            }

            if (m_on_stack) {
                m_storage.stack[m_size].~T();
            } else {
                m_storage.heap_data_ptr[m_size].~T();
            }

            --m_size;
        }

        size_t size() noexcept { return m_size; }
        const size_t  size() const noexcept { return m_size; }
        size_t  capacity() noexcept { return m_capacity; }
        const size_t capacity() const noexcept { return m_capacity; }

        bool on_stack() noexcept { return m_on_stack; }
        bool empty() noexcept { return m_size == 0; }
        void clear() noexcept {
            while (!empty()) {
                pop_back();
            }
         } 
};