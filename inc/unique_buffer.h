
// Drill 1 - Unique Buffer
// 	Modern C++ primitives — write UniqueBuffer<T> (move-only), unit-test copy elision & rule of 5

/* 
Spec: UniqueBuffer wraps a raw buffer + size; no copies, movable.

Tests: construction, move ctor leaves source null, destructor frees once.

Refactor: add reserve, resize, overload operator[].

Why: shows RAII, move semantics, and memory hygiene in one go.
*/

template <typename T>
class UniqueBuffer {
    public:
        UniqueBuffer<T>() : m_size(0), m_buffer(nullptr) {}

        UniqueBuffer<T>(size_t size): m_size(size), m_buffer(std::make_unique<T[]>(size)) { }
        
        UniqueBuffer<T>(UniqueBuffer<T>&& src) noexcept 
            : m_size(std::exchange(src.m_size, 0)),
              m_buffer(std::move(src.m_buffer)) {}

        UniqueBuffer<T>& operator=(UniqueBuffer<T>&& src) noexcept {
            if (this != &src) {
                m_buffer = std::move(src.m_buffer);            
                m_size = src.m_size;
                src.m_buffer = nullptr;
                src.m_size = 0;
            }
            return *this;
        }

        T& operator[](size_t i) {
            if (i >= m_size) throw std::out_of_range("index out of range for buffer");
            return m_buffer[i];
        }

        const T& operator[](size_t index) const {
            if (i >= m_size) throw std::out_of_range("index out of range for buffer");
            return &m_buffer[i];
        }

        // prevent copies
        UniqueBuffer<T> operator=(const UniqueBuffer<T>& copy) = delete;
        UniqueBuffer<T>(const UniqueBuffer<T>& copy) = delete;

    private:
        size_t m_size;
        std::unique_ptr<T[]> m_buffer;

    public:
        size_t size() noexcept { return m_size; }
        const size_t size() const noexcept { return m_size; }
        T* data() noexcept { return m_buffer.get(); }
        const T* data() const noexcept { return m_buffer.get(); }
};