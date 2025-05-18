
#include <unique_buffer.h>

template <typename T, size_t N>
class SmallVector {
    union VectorStore {
        T stack[N];
        UniqueBuffer<T> heap_buffer;

        VectorStore() {}
        ~VectorStore() {}
    };

    public:
        SmallVector<T, N>() : m_size(0), m_capacity(N) {}
        SmallVector<T, N>(size_t size) : 
                            m_size(size), 
                            m_capacity(m_size > N ? m_size + N : N) {}

        SmallVector<T, N>(size_t size, T default_value) : 
                            m_size(size), 
                            m_capacity(m_size > N ? m_size + N : N) {
            for (int i = 0; i < size; i++) {
                push_back(default_value);
            }
        }

        SmallVector<T, N>(std::initializer_list<T> init) : 
                        m_size(init.size()), 
                        m_capacity(m_size > N ? m_size + N : N) {
            
        }

        SmallVector<T, N>& operator=(SmallVector<T,N>&& src) noexcept {
            return *this;
        }

        SmallVector<T, N>(const SmallVector<T, N>& copy) {

        }

        ~SmallVector<T, N>() {
            // clean up the heap allocated nonsense!
        }

        T& operator[](size_t index) {
            if (index > m_capacity) throw std::out_of_range("index out of range for vector");
            return m_storage.stack[index];
        }

        const T& operator[](size_t index) const {
            if (index > m_capacity) throw std::out_of_range("index out of range for vector");
            return m_storage.stack[index];
        }

        SmallVector<T, N>& operator=(const SmallVector<T, N>& copy) {
            return *this;
        }

        T* begin() { return nullptr; }
        T* end() { return nullptr; }

    private:
        size_t m_size;
        size_t m_capacity;
        VectorStore m_storage;

    public:
        T at(size_t index) { return T(); }
        const T at(size_t index) const { return T(); }

        void push_back(T element) {
            if (m_size < N) {
                m_storage.stack[m_size] = element;
            } else {
                // todo: we need resize on UniqueBuffer
                m_storage.heap_buffer[m_size] = element;
            }
            m_size++;   
        }

        T pop_back() {
            T ret = {};
            return ret;
        }

        size_t size() noexcept { return m_size; }
        const size_t  size() const noexcept { return m_size; }
        size_t  capacity() noexcept { return m_capacity; }
        const size_t capacity() const noexcept { return m_capacity; }

        bool on_stack() noexcept { return false; }
        bool empty() noexcept { return m_size == 0; }
        void clear() noexcept { } 
};