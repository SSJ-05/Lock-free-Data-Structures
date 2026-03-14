// Ranked Mutex Hierarchical// 09.03.26// ZeroK

#pragma once

#include <thread>
#include <mutex>
#include <vector>


inline thread_local std::vector<int> local_stack;
class RankedMutex {
public:
    explicit RankedMutex(const int r) noexcept : _rank(r) {}

    RankedMutex (const RankedMutex&) = delete;
    RankedMutex& operator=(const RankedMutex&) = delete;

private:
    mutable std::mutex _m;
    const int _rank;

    friend class RankedLockGuard;

    void lock() const noexcept {
        if (!local_stack.empty()) {
            const int current = local_stack.back();
            if (_rank <= current) hierarchy_violation(_rank, current);
        }
        _m.lock();
        
        // push might throw (OOM), therefore we need to catch 
        try {
            local_stack.push_back(_rank);   // acquire 
        } catch (...) {
            _m.unlock();
            std::abort();
        }
    }

    void unlock() const noexcept {
        // unlock in strict LIFO order
        assert(!local_stack.empty());
        assert(local_stack.back() == _rank);    // inspect

        local_stack.pop_back();         // release
        _m.unlock();
    }

    [[ noreturn ]] static void hierarchy_violation (int attempted, int current) noexcept {
        std::fprintf(stderr, "Hierarchy Violation: holding %d while holding %d/n", attempted, current);
        std::abort();     // crash with core-dump, better than deadlock
    }
};

// custom RAII lock guard 
class RankedLockGuard {
public:
    explicit RankedLockGuard (const RankedMutex& m) : _m(m) { _m.lock(); }

    ~RankedLockGuard () noexcept { _m.unlock(); }

    RankedLockGuard (const RankedLockGuard&) = delete;
    RankedLockGuard& operator=(const RankedLockGuard&) = delete;

private:
    const RankedMutex& _m;
};

