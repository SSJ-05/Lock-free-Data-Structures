// Ring buffer version 1.0// 06.06.26// ZeroK

/* FEATURES:
 * think of array as ring - wrap back to idx 0 when you reach the end
 * fixed size - overwrite stale data
 * two pointers - head (producer) and tail (consumer)
 * buffer_size = power of 2 for wrap around logic to work
 * mask = buffer_size - 1
 * wrap around logic : actual idx = head & mask (bitwise & is faster than %)
 * head and tail ptrs will increment forever and wrap around - never capped
 * ptr positions to infer 
 *      head == tail                  :   empty buffer
 *      head - tail == buffer_size    :   full buffer
 *      head - tail                   :   pending data
 *      buffer [head & mask]          :   write data
 *      buffer [tail & mask]          :   read data
 ** above arithmetic is unsigned - works well even after out-of-bound     
 * */




#include <cstdint>
#include <cstdlib>

namespace zerok {

template <typename T, std::size_t Capacity>
class alignas (64) z_ring {

private:
    static_assert (Capacity > 0, "***capacity cannot be zero***");
    static_assert ( (Capacity & (Capacity - 1)) == 0, "***size must be power of 2***");

    static constexpr std::size_t MASK_  { Capacity - 1 };

    alignas (64) T            buffer_ [Capacity];
    alignas (64) std::size_t  head_   {};
    alignas (64) std::size_t  tail_   {};

public:

    // copy ctor
    z_ring  (const z_ring&)          = delete;
    z_ring& operator=(const z_ring&) = delete;
    
    // move ctor
    z_ring  (z_ring&&)          = delete;
    z_ring& operator=(z_ring&&) = delete;

    
    // operations
    // push
    [[ nodiscard ]]
    inline bool push (const T& value) noexcept {
        if (full ()) return false;

        buffer_[head_ & MASK_] = value;
        ++head_;
        return true;
    }

    // pop
    [[ nodiscard ]]
    inline bool pop (T& value) noexcept {
        if (empty ()) return false;

        value = buffer_[tail_ & MASK_];
        ++tail_;
        return true;
    }

    // full
    [[ nodiscard ]]
    inline bool full () const noexcept {
        return (head_ - tail_) == Capacity;
    }

    // empty
    [[ nodiscard ]]
    inline bool empty () const noexcept {
        return (head_ == tail_);
    }

    // size
    [[ nodiscard ]]
    inline std::size_t size () const noexcept {
        return head_ - tail_;
    }

    // capacity
    [[ nodiscard ]]
    constexpr std::size_t capacity () const noexcept {
        return Capacity;
    }

};

} // namespace zerok
