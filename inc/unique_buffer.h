
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
        UniqueBuffer<T>(const UniqueBuffer<T>& copy);
        UniqueBuffer<T>(UniqueBuffer<T>&& src);
        ~UniqueBuffer<T>();

        UniqueBuffer<T> operator=(const UniqueBuffer<T>& copy);
        UniqueBuffer<T> operator=(UniqueBuffer<T>&& src);
    private:
        
};