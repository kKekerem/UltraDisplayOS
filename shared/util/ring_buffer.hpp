#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <new>

// Cache line size to avoid false sharing
#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

namespace ud {

// Lock-free Single-Producer Single-Consumer ring buffer.
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) 
        : capacity_(capacity) {
        // Must be power of 2
        if ((capacity_ == 0) || ((capacity_ & (capacity_ - 1)) != 0)) {
            capacity_ = 1;
            while (capacity_ < capacity) capacity_ <<= 1;
        }
        mask_ = capacity_ - 1;
        buffer_.resize(capacity_);
    }

    bool try_push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next_head = (head + 1) & mask_;

        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // Full
        }

        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // Empty
        }

        item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return true;
    }

    void push_overwrite(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next_head = (head + 1) & mask_;

        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);

        // If we overflowed tail, bump it
        size_t tail = tail_.load(std::memory_order_acquire);
        if (next_head == tail) {
            tail_.store((tail + 1) & mask_, std::memory_order_release);
        }
    }

    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        if (head >= tail) return head - tail;
        return capacity_ - tail + head;
    }

private:
    size_t capacity_;
    size_t mask_;
    std::vector<T> buffer_;

    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
};

} // namespace ud
