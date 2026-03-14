// lock free queue// 03.03.26// ZeroK
#pragma once

#include <atomic>
#include <thread>
#include <memory>

template <typename T>
class spscq {
private:
    struct Node {
        std::shared_ptr<T> data_;
        Node* next_ { nullptr };
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

    Node* pop_head () {
        Node* const old_head = head_.load(std::memory_order_relaxed);

        if (old_head == tail_.load(std::memory_order_acquire)) return nullptr;
        head_.store(old_head->next_, std::memory_order_relaxed);
        
        return old_head;
    }

public:
    explicit spscq () : head_ (new Node), tail_ (head_.load()) {}

    ~spscq () noexcept {
        while (pop());
        delete head_.load();
    }

    spscq (const spscq&) = delete;
    spscq& operator=(const spscq&) = delete;
    spscq (spscq&&) = delete;
    spscq& operator=(spscq&&) = delete;


    std::shared_ptr<T> pop () {
        Node* const old_head = pop_head();
        if (!old_head) return std::shared_ptr<T>();

        std::shared_ptr<T> const result (old_head->data_);
        delete old_head;
        return result;
    }

    void push (T val) {
        std::shared_ptr<T> const new_data = std::make_shared<T>(std::move(val));
        Node* p = new Node;

        Node* const old_tail = tail_.load(std::memory_order_relaxed);
        old_tail->data_ = std::move(new_data);
        old_tail->next_ = p;

        // syncs with tail_.load() in pop_head()
        tail_.store(p, std::memory_order_release);
    }

};
