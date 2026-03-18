/**
 * buffer_pool.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_THREAD_SAFE_BUFFER_POOL_H
#define TERRAINER_THREAD_SAFE_BUFFER_POOL_H

#include <atomic>
#include <cstdlib>
#include <thread>
#include <memory>

#include "core/math/vector2i.h"
#include "core/templates/hash_map.h"

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#endif

namespace Terrainer {

/**
 *
 * BufferPool
 * A high-performance, lock-free buffer pool with thread-local caching.
 * Features:
 *   - Lock-free allocation and deallocation using atomic compare-and-swap
 *   - Thread-local caching to reduce contention on the global free list
 *   - Optimized memory ordering for minimal synchronization overhead
 *   - Reduced false sharing through strategic padding
 *   - Comprehensive statistics tracking with minimal performance impact
 *
 * This pool provides:
 * - O(1) allocation and deallocation in the common case
 * - Minimal lock contention through thread-local caches
 * - Cache-line aware design to minimize false sharing
 * - Comprehensive statistics with negligible overhead
 *
 * Template parameter T: Element type (e.g., uint16_t)
 */
template <typename T>
class BufferPool {
private:
    // Cache line size (commonly 64 bytes on modern systems)
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t THREAD_LOCAL_CACHE_SIZE = 16;

    struct FreeNode {
        FreeNode* next;
#ifdef TERRAINER_BUFFER_POOL_ENABLE_ABA_PREVENTION
        // ABA counter to prevent ABA problems
        uint32_t aba_tag;
#endif
    };

    // Aligned structure to prevent false sharing
    struct alignas(CACHE_LINE_SIZE) AlignedStats {
        std::atomic<size_t> allocated_count{0};
        std::atomic<size_t> peak_allocated{0};
        std::atomic<size_t> total_allocations{0};
        std::atomic<size_t> total_deallocations{0};
    };

    size_t block_size;
    size_t block_count;
    size_t alignment;
    size_t total_size;
    T *buffer;
    uintptr_t buffer_base;
    uintptr_t buffer_end;

    // Global free list head (accessed by all threads)
    std::atomic<FreeNode*> free_list_head{nullptr};

    // Statistics (padded to avoid false sharing)
    AlignedStats stats;

    // Thread-local cache structure
    struct ThreadLocalCache {
        FreeNode* local_cache[THREAD_LOCAL_CACHE_SIZE];
        size_t cache_count;

        ThreadLocalCache() : cache_count(0) {
            for (size_t i = 0; i < THREAD_LOCAL_CACHE_SIZE; ++i) {
                local_cache[i] = nullptr;
            }
        }
    };

    // Thread-local storage
    static thread_local ThreadLocalCache tls_cache;

public:
    /**
     * Constructor
     * \param p_block_size Number of elements per block
     * \param p_block_count Total number of blocks
     * \param p_alignment Alignment requirement in bytes (default: cache line size)
     */
    BufferPool(size_t p_block_size, size_t p_block_count, size_t p_alignment = 64)
        : block_size(_align_up(p_block_size, p_alignment))
        , block_count(p_block_count)
        , alignment(p_alignment)
    {
        total_size = block_size * block_count * sizeof(T);
        void *base;

#if defined(__ANDROID_API__) && (__ANDROID_API__ < 16)
        if (alignment < sizeof(void*)) {
            alignment = sizeof(void*);
        }
        base = memalign(alignment, total_size);
#elif defined(__APPLE__) || defined(__ANDROID__) || (defined(__linux__) && defined(__GLIBCXX__) && !defined(_GLIBCXX_HAVE_ALIGNED_ALLOC))
        if (alignment < sizeof(void*)) {
            alignment = sizeof(void*);
        }
        if (posix_memalign(&base, alignment, total_size) != 0) {
            base = nullptr;
        }
#elif defined(_WIN32)
        base = _aligned_malloc(total_size, alignment);
#elif __cplusplus >= 201703L || _MSVC_LANG >= 201703L
        base = aligned_alloc(alignment, total_size);
#else
#error "Aligned allocation not available."
#endif

        if (!base) {
            buffer = nullptr;
            return;
        }

        buffer = static_cast<T*>(base);
        buffer_base = reinterpret_cast<uintptr_t>(buffer);
        buffer_end = buffer_base + total_size;

        _build_free_list();
    }

    ~BufferPool() {
        if (buffer) {
#ifdef _WIN32
            _aligned_free(buffer);
#else
            free(buffer);
#endif
        }
    }

    // Non-copyable, non-movable
    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    BufferPool(BufferPool&&) = delete;
    BufferPool& operator=(BufferPool&&) = delete;

    /**
     * Allocate a block from the pool
     * Returns nullptr if no blocks are available
     */
    T* allocate() {
        // Try thread-local cache first (no atomics needed)
        if (tls_cache.cache_count > 0) {
            T* result = reinterpret_cast<T*>(tls_cache.local_cache[--tls_cache.cache_count]);
            stats.allocated_count.fetch_add(1, std::memory_order_relaxed);
            _update_peak();
            stats.total_allocations.fetch_add(1, std::memory_order_relaxed);
            return result;
        }

        // Try to allocate from global free list
        FreeNode* old_head = free_list_head.load(std::memory_order_acquire);

        while (old_head != nullptr) {
            FreeNode* new_head = old_head->next;

            if (free_list_head.compare_exchange_weak(
                    old_head, new_head,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                // Success! Update statistics
                stats.allocated_count.fetch_add(1, std::memory_order_relaxed);
                _update_peak();
                stats.total_allocations.fetch_add(1, std::memory_order_relaxed);
                return reinterpret_cast<T*>(old_head);
            }
        }

        // Out of blocks
        return nullptr;
    }

    /**
     * Allocate multiple blocks in batch (useful for bulk operations)
     * Returns number of blocks actually allocated
     */
    size_t allocate_batch(T** out_blocks, size_t count) {
        size_t allocated = 0;

        for (size_t i = 0; i < count; ++i) {
            out_blocks[i] = allocate();
            if (out_blocks[i] == nullptr) {
                break;
            }
            allocated++;
        }

        return allocated;
    }

    /**
     * Free a block back to the pool
     * Thread-safe, lock-free operation
     */
    void free(T* p_ptr) {
        if (!p_ptr || !owns(p_ptr)) {
            return;  // Invalid pointer
        }

        // Try to cache locally first (avoids atomics in fast path)
        if (tls_cache.cache_count < THREAD_LOCAL_CACHE_SIZE) {
            tls_cache.local_cache[tls_cache.cache_count++] = reinterpret_cast<FreeNode*>(p_ptr);
            stats.allocated_count.fetch_sub(1, std::memory_order_relaxed);
            stats.total_deallocations.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        // Local cache is full, flush half of it to global list
        _flush_local_cache(THREAD_LOCAL_CACHE_SIZE / 2);

        // Now add to local cache
        tls_cache.local_cache[tls_cache.cache_count++] = reinterpret_cast<FreeNode*>(p_ptr);
        stats.allocated_count.fetch_sub(1, std::memory_order_relaxed);
        stats.total_deallocations.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * Free multiple blocks in batch
     */
    void free_batch(T** p_blocks, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            free(p_blocks[i]);
        }
    }

    /**
     * Check if a pointer belongs to this pool
     */
    bool owns(const void* p_ptr) const {
        uintptr_t addr = reinterpret_cast<uintptr_t>(p_ptr);
        return addr >= buffer_base && addr < buffer_end;
    }

    /**
     * Flush all thread-local caches back to the global free list
     * Useful for cleanup or statistics accuracy
     */
    void flush_all_caches() {
        _flush_local_cache(tls_cache.cache_count);
    }

    // ========== Statistics API ==========

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
    size_t get_alignment() const { return alignment; }
    size_t get_thread_local_cache_size() const { return THREAD_LOCAL_CACHE_SIZE; }

    // ========== Debug API ==========

    /**
     * Get thread-local cache occupancy
     * Note: Only valid for current thread
     */
    size_t get_thread_cache_occupancy() const {
        return tls_cache.cache_count;
    }

private:
    /**
     * Build the initial free list
     */
    void _build_free_list() {
        T *block_ptr = buffer;

        for (size_t i = 0; i < block_count; ++i) {
            FreeNode* node = reinterpret_cast<FreeNode*>(block_ptr);
            node->next = (i + 1 < block_count) ? reinterpret_cast<FreeNode*>(block_ptr + block_size) : nullptr;
#ifdef TERRAINER_BUFFER_POOL_ENABLE_ABA_PREVENTION
            node->aba_tag = 0;
#endif
            block_ptr += block_size;
        }

        // Head points to first block
        free_list_head.store(reinterpret_cast<FreeNode*>(buffer), std::memory_order_release);
    }

    /**
     * Flush local cache entries back to the global free list
     */
    void _flush_local_cache(size_t count) {
        if (count == 0 || tls_cache.cache_count == 0) {
            return;
        }

        count = (count > tls_cache.cache_count) ? tls_cache.cache_count : count;

        // Build a chain of nodes to add
        FreeNode* chain_head = reinterpret_cast<FreeNode*>(tls_cache.local_cache[tls_cache.cache_count - 1]);
        FreeNode* chain_tail = chain_head;

        for (size_t i = tls_cache.cache_count - 2; i >= 0 && i < tls_cache.cache_count; --i) {
            FreeNode* node = reinterpret_cast<FreeNode*>(tls_cache.local_cache[i]);
            node->next = chain_tail;
            chain_tail = node;

            if (--count == 0) {
                break;
            }
        }

        // Atomically insert chain into global list
        FreeNode* old_head = free_list_head.load(std::memory_order_acquire);
        do {
            chain_head->next = old_head;
        } while (!free_list_head.compare_exchange_weak(
            old_head, chain_tail,
            std::memory_order_release,
            std::memory_order_acquire));

        // Update local cache
        tls_cache.cache_count -= (count > 0 ? count : 1);
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

    /**
     * Align size up to the nearest multiple of alignment
     */
    static constexpr size_t _align_up(size_t size, size_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

// Thread-local storage initialization
template <typename T>
thread_local typename BufferPool<T>::ThreadLocalCache BufferPool<T>::tls_cache;

} // namespace Terrainer

#endif // TERRAINER_THREAD_SAFE_BUFFER_POOL_H
