// Ring buffer version 1.1// 06.06.26// ZeroK

/* FEATURES:
 * think of array as ring - wrap back to idx 0 when you reach the end
 * fixed size - overwrite stale data
 * two pointers - head (producer) and tail (consumer)
 * buffer_size = power of 2 for wrap around logic (bitwise &) to work
 * mask = buffer_size - 1
 * wrap around logic : actual idx = head & mask (bitwise & is faster than modulo %)
 * head and tail ptrs will increment forever and wrap around - never capped
 * ptr positions to infer 
 *      head == tail                  :   empty buffer
 *      head - tail >= buffer_size    :   full buffer
 *      head - tail                   :   pending data
 *      buffer [head & mask]          :   write data
 *      buffer [tail & mask]          :   read data
 ** above arithmetic is unsigned - works well even after out-of-bound     
 * */


/* Bitwise logic
 * if n & (n-1) == 0, n is a power of 2
 * */

#pragma once

#include <cstdint>
#include <cstdlib>


namespace zerok {

template <typename T, std::size_t Capacity>
class alignas(64) z_ring {

private:
    static_assert (Capacity > 0, "***capacity cannot be zero***");
    static_assert ( (Capacity & (Capacity - 1)) == 0, "***size must be power of 2***");

    static constexpr std::size_t MASK_  { Capacity - 1 };

    alignas(64) T     buffer_ [Capacity];      // T = trivial POD types

    alignas(64) std::size_t    head_   {};
    char pad1_ [64 - sizeof(std::size_t)];      

    alignas(64) std::size_t    tail_   {};
    char pad2_ [64 - sizeof(std::size_t)];

public:

    z_ring()  = default;
    ~z_ring() = default;

    // copy ctor
    z_ring  (const z_ring&)          = delete;
    z_ring& operator=(const z_ring&) = delete;
    
    // move ctor
    z_ring  (z_ring&&)          = delete;
    z_ring& operator=(z_ring&&) = delete;

    
    // operations : push, pop, peek, reset
    // push
    [[ nodiscard ]]
    bool push (const T& value) noexcept {
        if (full ()) return false;

        buffer_[head_ & MASK_] = value;
        ++head_;
        return true;
    }

    // pop
    [[ nodiscard ]]
    bool pop (T& value) noexcept {
        if (empty ()) return false;

        value = buffer_[tail_ & MASK_];
        buffer_[tail_ & MASK_] = T{};       // optional: overwrite the slot with default
        ++tail_;
        return true;
    }

    
    // peek at elements
    [[ nodiscard ]]
    bool peek (T& value) const noexcept {
        if (empty ()) return false;
        
        value = buffer_ [tail_ & MASK_];
        return true;
    }

    // reset
    void reset () noexcept {
        head_ = 0;
        tail_ = 0;
    }



    // zero copy access for performance
    T* get_tail () const noexcept { return buffer_ + (tail_ & MASK_); }
    void advance_tail (std::size_t n) noexcept { tail_ += n; }



    // status checks - full, empty, size, capacity
    [[ nodiscard ]] bool full ()  const noexcept { return (head_ - tail_) >= Capacity; }
    [[ nodiscard ]] bool empty () const noexcept { return (head_ == tail_); }
    [[ nodiscard ]] std::size_t size () const noexcept { return head_ - tail_; }
    [[ nodiscard ]] constexpr std::size_t capacity () const noexcept { return Capacity; }

};

} // namespace zerok
