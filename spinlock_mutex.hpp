// spinlock mutex// 04.03.26// ZeroK
// uses TTAS for high contention scenarios
// TTAS - Test Test And Set
// reduces cache coherence traffic by spinning on relaxed loads (shared MESI state)
// before attempting expensive RMW operation (test_and_set)

#pragma once

#include <atomic>
#include <thread>
#include <xmmintrin.h>
#include <cstdint>

class spinlock_mutex2 {
private:
    std::atomic_flag flag_ { ATOMIC_FLAG_INIT };
    static constexpr std::uint8_t spin_limit_ { 0x64 };

public:
    spinlock_mutex2 () = default;
    ~spinlock_mutex2 () = default;

    void lock() noexcept {
        std::uint8_t spin_count { 0x0 };

        while (true) {
            // phase 1: read only spin - test first
            // cache line stays in Shared MESI state across the cores
            // no RFO requests = no coherence traffic
            while (flag_.test(std::memory_order_relaxed)) {
                if (++spin_count > spin_limit_) {
                    std::this_thread::yield();  // yield to OS after spinning
                    spin_count = 0x0;
                }
                else _mm_pause();   // hint CPU to reduce power
            }

            // phase 2: try acquire - test and set
            // if lock appears free, attempt RMW op
            if (!flag_.test_and_set(std::memory_order_acquire)) return; // success

            spin_count = 0x0; // reset when fail
        }
    }

    void unlock() noexcept {
        flag_.clear(std::memory_order_release);
    }

    // non copyable, non movable
    spinlock_mutex2 (const spinlock_mutex2&) = delete;
    spinlock_mutex2& operator=(const spinlock_mutex2&) = delete;
    spinlock_mutex2 (spinlock_mutex2&&) = delete;
    spinlock_mutex2& operator=(spinlock_mutex2&&) = delete;

};

