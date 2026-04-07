// UHFT hugepage backed arena// 06.04.26// ZeroK 

#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/mman.h>   // for mmap, munmap, mlock, munlock
#include <cerrno>       // for fprintf error context
#include <cstdio>       // for fprintf
#include <new>
#include <type_traits>

namespace zerok {

class Arena {
private:
    // hot data - accessed on every allocate
    alignas (64) std::size_t offset_ { 0uz };
    std::uint8_t pad_ [56];
    
    // cold data - on separate cache line - avoid false sharing
    std::size_t size_;
    std::uint8_t* memory_;

public:
    explicit Arena (std::size_t size) noexcept : size_(size) {
        // Hugepage backed and lock memory
        memory_ = static_cast<uint8_t*>(mmap (  nullptr, size,
                                                PROT_READ | PROT_WRITE, 
                                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                                                -1, 0));

        // fallback to normal pages and lock memory
        if (memory_ == MAP_FAILED) {
            memory_ = nullptr;
            fprintf(stderr, "[Arena] FATAL : hugepage allocation failed.\n"
                            "Run : echo N | sudo tee /proc/sys/vm/nr_hugepages\n");
            std::terminate();
        }

        // lock memory 
        if (mlock(memory_, size) != 0) { 
            fprintf(stderr, "[Arena] FATAL : mlock failed.\n"
                            "Check R_LIMIT_MEMLOCK or run as root.\n");
            munmap(memory_, size_);
            memory_ = nullptr;
            std::terminate();
        }

        // pre-touch pages
        touch_all_pages(); 
    }

    ~Arena() {
        if (memory_ && memory_ != MAP_FAILED) {
            munlock(memory_, size_);
            munmap(memory_, size_);
            memory_ = nullptr;
        }
    }
    // these processes should only happen at system shutdown, never in hot path

    // no exceptions allocation, with default cache line 64 bytes
    [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment = 64) noexcept {
        uintptr_t current = reinterpret_cast<uintptr_t>(memory_ + offset_);
        uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
        std::size_t padding = aligned - current;

        if (offset_ + padding + size >  size_) return nullptr;

        offset_ += padding;
        void* ptr = memory_ + offset_;
        offset_ += size;
        return ptr;
    }

    // create order
    template<typename T, typename... Args>
    T* create_order(Args&&... args) noexcept (std::is_nothrow_constructible_v<T, Args...>) {
        void* ptr = allocate(sizeof(T), alignof(T));
        if (!ptr) return nullptr;
        return new (ptr) T(std::forward<Args>(args)...);
    }

    // pre-touch pages
    void touch_all_pages() noexcept {
        constexpr std::size_t page_size = 2 * 1024 * 1024;   // pre-touch 2MB pages
        volatile std::uint8_t* mem = memory_;

        for (auto i {0uz}; i < size_; i += page_size) {
            mem[i] = 0; 
    
            std::size_t last_byte = i + page_size - 1;
            if (last_byte < size_) mem[last_byte] = 0;
        }
    }

    // reset without calling destructor (PREFERRED IN HOT PATH)
    void reset() noexcept { offset_ = 0; }

    // statistics
    std::size_t used() const noexcept { return offset_; }
    std::size_t available() const noexcept { return size_ - offset_; }
    std::size_t capacity() const noexcept { return size_; }

    // prevent copying/moving
    Arena (const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena (Arena&&) noexcept = delete;
    Arena& operator=(Arena&&) noexcept = delete;
};

} // namespace zerok
// first run this in CLI to preallocate '20' hugepages: sudo sh -c 'echo 20 > /proc/sys/vm/nr_hugepages'
// or echo 20 | sudo tee /proc/sys/vm/nr_hugepages
// for reset use : echo 0 | sudo tee /proc/sys/vm/nr_hugepages
// for info : grep Huge /proc/meminfo
// check which hugepage size is available in linux : ls /sys/kernel/mm/hugepages/  
// set the hugepage size to 2MB : echo 20 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

