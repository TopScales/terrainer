/**
 * aligned_buffer.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_ALIGNED_BUFFER_H
#define TERRAINER_ALIGNED_BUFFER_H

#include <atomic>
#include <cstdlib>
#include <thread>
#include <memory>

#include "core\typedefs.h"

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#endif

namespace Terrainer {

template <typename T>
class AlignedBuffer {
private:
    size_t block_size;
    size_t alignment;
    T *buffer;

public:
    AlignedBuffer(size_t p_block_size, size_t p_alignment = 64)
        : block_size(_align_up(p_block_size, p_alignment))
        , alignment(p_alignment)
    {
        size_t total_size = block_size * sizeof(T);
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
    }

    ~AlignedBuffer() {
        if (buffer) {
#ifdef _WIN32
            _aligned_free(buffer);
#else
            free(buffer);
#endif
        }
    }

    // Non-copyable, non-movable
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;
    AlignedBuffer(AlignedBuffer&&) = delete;
    AlignedBuffer& operator=(AlignedBuffer&&) = delete;

    _FORCE_INLINE_ T *ptrw() { return buffer; }
	_FORCE_INLINE_ const T *ptr() const { return buffer; }

    size_t get_block_size() const { return block_size; }
    size_t get_total_size() const { return block_size * sizeof(T); }

private:

    /**
     * Align size up to the nearest multiple of alignment
     */
    static constexpr size_t _align_up(size_t size, size_t alignment) {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace Terrainer

#endif // TERRAINER_ALIGNED_BUFFER_H
