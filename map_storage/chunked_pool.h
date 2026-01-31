/**
 * chunked_pool.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_CHUNKED_POOL_H
#define TERRAINER_CHUNKED_POOL_H

#include <atomic>
#include <cstdlib>

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#endif

namespace Terrainer {

class ChunkedPool {
private:
    size_t chunk_size;
    size_t chunk_count;
    size_t alignment;
    size_t total_size;
    uint8_t *buffer;

    struct FreeNode {
        FreeNode* next;
    };
    std::atomic<FreeNode*> free_list_head{nullptr};

    std::atomic<size_t> allocated_count{0};
    std::atomic<size_t> peak_allocated{0};

public:
    ChunkedPool(size_t p_chunk_size, size_t p_chunk_count, size_t p_alignment = 64)
        : chunk_size(align_up(p_chunk_size, p_alignment))
        , chunk_count(p_chunk_count)
        , alignment(p_alignment)
    {
        total_size = chunk_size * chunk_count;
        void *base;

#if defined(__ANDROID_API__) && (__ANDROID_API__ < 16)
        // Alignment must be >= sizeof(void*).
        if (alignment < sizeof(void*))
        {
            alignment = sizeof(void*);
        }

        base = memalign(alignment, total_size);
#elif defined(__APPLE__) || defined(__ANDROID__) || (defined(__linux__) && defined(__GLIBCXX__) && !defined(_GLIBCXX_HAVE_ALIGNED_ALLOC))
        // Unfortunately, aligned_alloc causes VMA to crash due to it returning null pointers. (At least under 11.4)
        // Therefore, for now disable this specific exception until a proper solution is found.
        //#if defined(__APPLE__) && (defined(MAC_OS_X_VERSION_10_16) || defined(__IPHONE_14_0))
        //#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_16 || __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
        //    // For C++14, usr/include/malloc/_malloc.h declares aligned_alloc()) only
        //    // with the MacOSX11.0 SDK in Xcode 12 (which is what adds
        //    // MAC_OS_X_VERSION_10_16), even though the function is marked
        //    // available for 10.15. That is why the preprocessor checks for 10.16 but
        //    // the __builtin_available checks for 10.15.
        //    // People who use C++17 could call aligned_alloc with the 10.15 SDK already.
        //    if (__builtin_available(macOS 10.15, iOS 13, *))
        //        return aligned_alloc(alignment, size);
        //#endif
        //#endif

        // Alignment must be >= sizeof(void*).
        if (alignment < sizeof(void*))
        {
            alignment = sizeof(void*);
        }

        if (posix_memalign(&buffer, alignment, total_size) != 0) {
            base = nullptr;
        }
#elif defined(_WIN32)
        base = _aligned_malloc(total_size, alignment);
#elif __cplusplus >= 201703L || _MSVC_LANG >= 201703L // C++17
        base = aligned_alloc(alignment, total_size);
#else
#error "Aligned allocation not available."
#endif

        if (!base) {
            return;
        }

        buffer = static_cast<uint8_t*>(base);
        build_free_list();
    }

    ~ChunkedPool() {
        if (buffer) {
#ifdef _WIN32
            _aligned_free(buffer);
#else
            free(buffer);
#endif

        }
    }

    ChunkedPool(const ChunkedPool&) = delete;
    ChunkedPool& operator=(const ChunkedPool&) = delete;

    uint8_t* allocate() {
        FreeNode* old_head = free_list_head.load(std::memory_order_acquire);

        while (old_head != nullptr) {
            FreeNode* new_head = old_head->next;

            if (free_list_head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_acquire)) {
                // Success! Track stats.
                size_t current = allocated_count.fetch_add(1, std::memory_order_relaxed) + 1;

                // Update peak (racy but fine for stats).
                size_t peak = peak_allocated.load(std::memory_order_relaxed);
                while (current > peak) {
                    if (peak_allocated.compare_exchange_weak(peak, current)) {
                        break;
                    }
                }

                return (uint8_t*)old_head;
            }
        }

        // Out of chunks!
        return nullptr;
    }

    // Free a chunk (thread-safe, lock-free)
    void free(uint8_t* ptr) {
        if (!ptr || !owns(ptr)) {
            return;  // Invalid pointer
        }

        // Convert to free node
        FreeNode* node = reinterpret_cast<FreeNode*>(ptr);
        FreeNode* old_head = free_list_head.load(std::memory_order_acquire);

        do {
            node->next = old_head;
        } while (!free_list_head.compare_exchange_weak(old_head, node, std::memory_order_release, std::memory_order_acquire));

        allocated_count.fetch_sub(1, std::memory_order_relaxed);
    }

    // Check if pointer belongs to this pool
    bool owns(void* ptr) const {
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t base = reinterpret_cast<uintptr_t>(buffer);
        return addr >= base && addr < (base + total_size);
    }

    // Stats
    size_t get_allocated_count() const {
        return allocated_count.load(std::memory_order_relaxed);
    }

    size_t get_free_count() const {
        return chunk_count - allocated_count.load(std::memory_order_relaxed);
    }

    size_t get_peak_allocated() const {
        return peak_allocated.load(std::memory_order_relaxed);
    }

    float get_utilization() const {
        return (float)allocated_count.load(std::memory_order_relaxed) / chunk_count;
    }

    // Configuration accessors
    size_t get_chunk_size() const { return chunk_size; }
    size_t get_chunk_count() const { return chunk_count; }
    size_t get_total_size() const { return total_size; }

private:
    void build_free_list() {
        uint8_t *chunk_ptr = buffer;

        for (size_t i = 0; i < chunk_count; ++i) {
            FreeNode* node = reinterpret_cast<FreeNode*>(chunk_ptr);
            node->next = (i + 1 < chunk_count) ? reinterpret_cast<FreeNode*>(chunk_ptr + chunk_size) : nullptr;
            chunk_ptr += chunk_size;
        }

        // Head points to first chunk
        free_list_head.store(reinterpret_cast<FreeNode*>(buffer), std::memory_order_release);
    }

    static size_t align_up(size_t size, size_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace Terrainer

#endif // TERRAINER_CHUNKED_POOL_H