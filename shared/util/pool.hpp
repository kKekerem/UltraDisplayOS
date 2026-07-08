#pragma once

#include <atomic>
#include <cstdint>
#include <new>
#include <vector>
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace ud {

template <typename T>
class Pool;

template <typename T>
class PoolPtr {
public:
    PoolPtr() : pool_(nullptr), ptr_(nullptr) {}
    PoolPtr(Pool<T>* pool, T* ptr) : pool_(pool), ptr_(ptr) {}
    
    ~PoolPtr() {
        if (ptr_ && pool_) {
            pool_->free(ptr_);
        }
    }

    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;

    PoolPtr(PoolPtr&& other) noexcept : pool_(other.pool_), ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_ && pool_) {
                pool_->free(ptr_);
            }
            pool_ = other.pool_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

private:
    Pool<T>* pool_;
    T* ptr_;
};

// Fixed size, zero-allocation memory pool.
template <typename T>
class Pool {
public:
    explicit Pool(size_t capacity) : capacity_(capacity) {
        size_t block_size = sizeof(Node);
        size_t total_size = capacity * block_size;
        
#ifdef _WIN32
        memory_ = VirtualAlloc(nullptr, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!memory_) throw std::bad_alloc();
#else
        memory_ = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (memory_ == MAP_FAILED) throw std::bad_alloc();
#endif

        auto* nodes = static_cast<Node*>(memory_);
        for (size_t i = 0; i < capacity - 1; ++i) {
            nodes[i].next = &nodes[i + 1];
        }
        nodes[capacity - 1].next = nullptr;
        head_.store(&nodes[0], std::memory_order_relaxed);
    }

    ~Pool() {
        size_t total_size = capacity_ * sizeof(Node);
#ifdef _WIN32
        VirtualFree(memory_, 0, MEM_RELEASE);
#else
        munmap(memory_, total_size);
#endif
    }

    PoolPtr<T> alloc() {
        Node* current_head = head_.load(std::memory_order_acquire);
        while (current_head != nullptr) {
            if (head_.compare_exchange_weak(current_head, current_head->next, 
                                            std::memory_order_release, 
                                            std::memory_order_relaxed)) {
                // Construct T
                T* ptr = new (&current_head->data) T();
                return PoolPtr<T>(this, ptr);
            }
        }
        return PoolPtr<T>(nullptr, nullptr); // Exhausted
    }

    void free(T* ptr) {
        if (!ptr) return;
        ptr->~T(); // Destroy T
        
        Node* node = reinterpret_cast<Node*>(ptr);
        Node* current_head = head_.load(std::memory_order_relaxed);
        do {
            node->next = current_head;
        } while (!head_.compare_exchange_weak(current_head, node, 
                                               std::memory_order_release, 
                                               std::memory_order_relaxed));
    }

private:
    struct alignas(T) Node {
        union {
            uint8_t data[sizeof(T)];
            Node* next;
        };
    };

    size_t capacity_;
    void* memory_;
    std::atomic<Node*> head_;
};

} // namespace ud
