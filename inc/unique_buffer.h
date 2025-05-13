
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
        
        UniqueBuffer<T>(UniqueBuffer<T>&& src) noexcept {
            m_buffer = std::move(src.m_buffer);
            m_size = src.m_size;
            src.m_buffer = nullptr;
            src.m_size = 0;
        }

        ~UniqueBuffer<T>() {
            m_size = 0;
        }

        UniqueBuffer<T>& operator=(UniqueBuffer<T>&& src) noexcept {
            m_buffer = std::move(src.m_buffer);            
            m_size = src.m_size;
            src.m_buffer = nullptr;
            src.m_size = 0;
            return *this;
        }

        // prevent copies
        UniqueBuffer<T> operator=(const UniqueBuffer<T>& copy) = delete;
        UniqueBuffer<T>(const UniqueBuffer<T>& copy) = delete;

    private:
        size_t m_size;
        std::unique_ptr<T[]> m_buffer;

    public:
        const size_t size() { return m_size; }
        const T* data() { return m_buffer.get(); }
};