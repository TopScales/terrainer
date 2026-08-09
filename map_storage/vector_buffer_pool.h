/**
 * vector_buffer_pool.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_VECTOR_BUFFER_POOL_H
#define TERRAINER_VECTOR_BUFFER_POOL_H

#include <atomic>
// #include "templates\vector.h"

namespace Terrainer {

template <typename T>
// typedef uint16_t T;
class VectorBufferPool {
private:
    // Cache line size (commonly 64 bytes on modern systems)
    static constexpr size_t CACHE_LINE_SIZE = 64;

    // Aligned structure to prevent false sharing
    struct alignas(CACHE_LINE_SIZE) AlignedStats {
        std::atomic<size_t> allocated_count{0};
        std::atomic<size_t> peak_allocated{0};
        std::atomic<size_t> total_allocations{0};
        std::atomic<size_t> total_deallocations{0};
    };

    size_t block_size;
    size_t block_count;
    size_t total_size;
    Vector<Vector<T>> buffers;

    // Global free list head (accessed by all threads)
    std::atomic<int64_t> free_list_head{0};

    // Statistics (padded to avoid false sharing)
    AlignedStats stats;

public:
    static const int64_t INVALID_BUFFER = -1;

    VectorBufferPool(size_t p_block_size, size_t p_block_count)
        : block_size(p_block_size)
        , block_count(p_block_count)
    {
        total_size = block_size * block_count * sizeof(T);
        Error err = buffers.resize(block_count);
        ERR_FAIL_COND_EDMSG(err != OK, "Failed to allocate buffers array.");

        for (int64_t i = 0; i < block_count; ++i) {
            err = buffers.write[i].resize(block_size);
            ERR_FAIL_COND_EDMSG(err != OK, "Failed to allocate buffers.");
        }

        _build_free_list();
    }

    ~VectorBufferPool() {
        for (int64_t i = 0; i < buffers.size(); ++i) {
            buffers.write[i].clear();
        }

        buffers.clear();
    }

    // Non-copyable, non-movable
    VectorBufferPool(const VectorBufferPool&) = delete;
    VectorBufferPool& operator=(const VectorBufferPool&) = delete;
    VectorBufferPool(VectorBufferPool&&) = delete;
    VectorBufferPool& operator=(VectorBufferPool&&) = delete;

    /**
     * Allocate a block from the pool
     * Returns INVALID_BUFFER if no blocks are available
     */
    int64_t allocate() {
        int64_t old_head = free_list_head.load(std::memory_order_acquire);

        while (old_head != INVALID_BUFFER) {
            int64_t new_head = *reinterpret_cast<int64_t*>(buffers.write[old_head].ptrw());

            if (free_list_head.compare_exchange_weak(
                    old_head, new_head,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                // Success! Update statistics
                stats.allocated_count.fetch_add(1, std::memory_order_relaxed);
                _update_peak();
                stats.total_allocations.fetch_add(1, std::memory_order_relaxed);
                return old_head;
            }
        }

        // Out of blocks
        return INVALID_BUFFER;
    }

    /**
     * Free a block back to the pool
     * Thread-safe, lock-free operation
     */
    void free(int64_t p_buffer_index) {
        if (p_buffer_index < 0 || p_buffer_index >= buffers.size()) {
            return;
        }

        int64_t *node = reinterpret_cast<int64_t*>(buffers.write[p_buffer_index].ptrw());
        int64_t old_head = free_list_head.load(std::memory_order_acquire);

        do {
            *node = old_head;
        } while (!free_list_head.compare_exchange_weak(
            old_head, p_buffer_index,
            std::memory_order_release,
            std::memory_order_acquire));

        stats.allocated_count.fetch_sub(1, std::memory_order_relaxed);
        stats.total_deallocations.fetch_add(1, std::memory_order_relaxed);
    }

    _FORCE_INLINE_ T *ptrw(int64_t p_buffer_index) { return buffers.write[p_buffer_index].ptrw(); }
    _FORCE_INLINE_ const T *ptr(int64_t p_buffer_index) const { return buffers.get(p_buffer_index).ptr(); }
    _FORCE_INLINE_ const Vector<T> &get(int64_t p_buffer_index) const { return buffers.get(p_buffer_index); }

    /**
     * Get number of currently allocated blocks
     */
    size_t get_allocated_count() const {
        return stats.allocated_count.load(std::memory_order_relaxed);
    }

    /**
     * Get number of currently free blocks
     */
    size_t get_free_count() const {
        return block_count - stats.allocated_count.load(std::memory_order_relaxed);
    }

    /**
     * Get peak allocated count since pool creation
     */
    size_t get_peak_allocated() const {
        return stats.peak_allocated.load(std::memory_order_relaxed);
    }

    /**
     * Get total allocations performed (cumulative)
     */
    size_t get_total_allocations() const {
        return stats.total_allocations.load(std::memory_order_relaxed);
    }

    /**
     * Get total deallocations performed (cumulative)
     */
    size_t get_total_deallocations() const {
        return stats.total_deallocations.load(std::memory_order_relaxed);
    }

    /**
     * Get pool utilization as a percentage [0.0, 1.0]
     */
    float get_utilization() const {
        return static_cast<float>(stats.allocated_count.load(std::memory_order_relaxed)) / block_count;
    }

    /**
     * Get available memory in blocks
     */
    size_t get_available_blocks() const {
        return block_count - stats.allocated_count.load(std::memory_order_relaxed);
    }

    /**
     * Get available memory in bytes
     */
    size_t get_available_bytes() const {
        return get_available_blocks() * block_size * sizeof(T);
    }

    // ========== Configuration Accessors ==========

    size_t get_block_size() const { return block_size; }
    size_t get_block_count() const { return block_count; }
    size_t get_total_size() const { return total_size; }

private:
    /**
     * Build the initial free list
     */
    void _build_free_list() {
        for (int64_t i = 0; i < block_count - 1; ++i) {
            int64_t *node = reinterpret_cast<int64_t*>(buffers.write[i].ptrw());
            *node = i + 1;
        }

        int64_t *last = reinterpret_cast<int64_t*>(buffers.write[block_count - 1].ptrw());
        *last = INVALID_BUFFER;

        // Head points to first block
        free_list_head.store(0, std::memory_order_release);
    }

    /**
     * Update peak allocation count (racy but acceptable for stats)
     */
    void _update_peak() {
        size_t current = stats.allocated_count.load(std::memory_order_relaxed);
        size_t peak = stats.peak_allocated.load(std::memory_order_relaxed);

        while (current > peak) {
            if (stats.peak_allocated.compare_exchange_weak(
                    peak, current,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed)) {
                break;
            }
        }
    }
};
} // namespace Terrainer

#endif // TERRAINER_VECTOR_BUFFER_POOL_H
