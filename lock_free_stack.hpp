// threadsafe lock free stack// 03.03.26// ZeroK 
// linked list under the hood
// intentional memory leak in pop() - nodes are never deleted
#pragma once

#include <atomic>
#include <thread>
#include <memory>

template <typename T>
class lf_stack_leak {
private:
    struct Node {
        // T data_;
        std::shared_ptr<T> data_;
        Node* next_;
        Node (T const& data) : data_ (std::make_shared<T>(data)) {}
    };
    std::atomic<Node*> head_ { nullptr };
    static constexpr std::uint8_t spin_limit_ { 0x64 };

public:
    void push (T val) noexcept {
        Node* const new_node = new Node (std::move(val));
        new_node->next_ = head_.load(std::memory_order_relaxed);

        // limited spin, return on success
        for (std::uint8_t i {0}; i < spin_limit_; ++i) {
            if (head_.compare_exchange_weak(new_node->next_, new_node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
            { return; }     // success
            _mm_pause();
        }

        // if still contended, yield() to OS
        while (!head_.compare_exchange_weak(new_node->next_, new_node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed)) 
        { std::this_thread::yield(); }
    }

    std::shared_ptr<T> pop () {
        Node* old_head = head_.load(std::memory_order_relaxed);
        
        while (old_head && 
                !head_.compare_exchange_weak(old_head, old_head->next_,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) 
        { _mm_pause(); }

        return old_head ? old_head->data_ : std::shared_ptr<T>();
    }

    lf_stack_leak (const lf_stack_leak&) = delete;
    lf_stack_leak& operator=(const lf_stack_leak&) = delete;
    lf_stack_leak (lf_stack_leak&&) = delete;
    lf_stack_leak& operator=(lf_stack_leak&&) = delete;

};

