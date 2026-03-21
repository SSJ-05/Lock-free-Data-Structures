/* lock free queue// 03.03.26// ZeroK
 * new features added on 21.03.26
 * 1. data_ is no longer accessed thru shared_ptr
 * 2. deployed custom memory pool 
 * replaced shared_ptr with std::optional in pop() 
                
*/


#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <cstdint>

namespace zerok {

template <typename T, std::size_t Capacity>
class spscq {
private:

    // Node - linked list
    struct Node {
        T data_
        Node* next_ { nullptr };
    };


    // Queue internals
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

    Node* pop_head () {
        Node* const old_head = head_.load(std::memory_order_relaxed);

        if (old_head == tail_.load(std::memory_order_acquire)) return nullptr;
        head_.store(old_head->next_, std::memory_order_relaxed);
        
        return old_head;
    }


    // Custom memory pool - yeah!
    alignas(Node) std::uint8_t pool_storage_ [Capacity * sizeof(Node)];     // fixed storage + alignment

    Node* producer_free_list_ { nullptr };             // ptr used by producer 
    Node* consumer_free_list_ { nullptr };             // ptr used by consumer
    std::atomic<Node*> returned_list_ { nullptr };     // atomic ptr for bulk returns to producer                                

    // called by producer only
    Node* pool_acquire() {
        // 1. try producer list
        if (producer_free_list_) {
            Node* n = producer_free_list_;
            producer_free_list_ = n->next_;
            return n;
        }

        // 2. drain returned_list
        Node* returned = returned_list_.exchange(nullptr, 
                                std::memory_order_acquire);
        
        if (!returned) return nullptr;  // pool full
        
        // 3. returned_list becomes the new producer list
        producer_free_list_ = returned->next_;
        returned->next_ = nullptr;
        return returned;
    }

    // called by consumer only
    void pool_release (Node* n) {
        // push to returned_list atomically
        n->next_ = returned_list_.load(std::memory_order_relaxed);
        while (!returned_list_.compare_exchange_weak(n->next_, n,
                                                std::memory_order_release, 
                                                std::memory_order_relaxed));
    }

    void init_pool () {
        Node* nodes = reinterpret_cast<Node*>(pool_storage_);
        
        for (auto i {0uz}; i < Capacity - 1; ++i) {
            nodes[i].next_ = &nodes[i + 1];
        }
    
        nodes[Capacity - 1].next_ = nullptr;
        producer_free_list_ = &nodes[0];    // producer owns all nodes
    }


public:
    explicit spscq () {
        init_pool();
        Node* dummy = pool_acquire();   // dummy node
        dummy->next_ = nullptr;
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }

    ~spscq () noexcept {
        while (pop());
        pool_release(head_.load());     // release the dummy node
    }

    spscq (const spscq&) noexcept = delete;
    spscq& operator=(const spscq&) noexcept = delete;
    spscq (spscq&&) noexcept = delete;
    spscq& operator=(spscq&&) noexcept = delete;


    // no heap allocation
    // called by consumer thread only
    std::optional<T> pop () {
        Node* const old_head = pop_head();
        if (!old_head) return std::nullopt;

        std::optional<T> result { std::move(old_head->data_) };
        pool_release(old_head);       // return to pool, no 'delete' 
        return result;
    }

    // called by producer thread only
    void push (T val) {
        Node* p = pool_acquire();     // dummy node
        if (!p) return;               // if pool full, return
        p->data_ = std::move(val);
        p->next_ = nullptr;

        Node* const old_tail = tail_.load(std::memory_order_relaxed);
        old_tail->next_ = p;
        tail_.store(p, std::memory_order_release);
    }

};

} // namespace zerok
