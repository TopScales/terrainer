/**
 * lod_buffer.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_LOD_BUFFER_H
#define TERRAINER_LOD_BUFFER_H

// #include <atomic>
// #include <cstdlib>
// #include <thread>
// #include <memory>

#include "../utils/math.h"

namespace Terrainer {

enum class LODBUfferSection {
    Main = -1,
    XNegPad,
    XPosPad,
    ZNegPad,
    ZPosPad
};

template <typename T>
struct LODBufferSpecs {
    const size_t block_side;
    const size_t pages;
    int lods;
    bool use_padding;
    size_t page_size = 0;
    size_t* lod_offsets = nullptr;
    size_t* lod_sides = nullptr;

    LODBufferSpecs(size_t p_block_side, size_t p_pages, int p_lods, bool p_use_padding = false)
        : block_side(p_block_side), pages(p_pages), lods(p_lods), use_padding(p_use_padding)
    {
        page_size = use_padding ? lod_geom_expand_sqr(block_side * block_side, lods) + lod_geom_expand(6 * block_side, lods) + 5 * lods
            : lod_geom_expand_sqr(block_side * block_side, lods);
        size_t total_size = page_size * pages * sizeof(T);
        lod_offsets = (size_t*)memalloc((lods + 1) * sizeof(size_t));
        lod_sides = (size_t*)memalloc(lods * sizeof(size_t));

        if (use_padding) {
            size_t offset = 0;
            size_t side = block_side;

            for (int ilod = 0; ilod < lods; ++ilod) {
                lod_offsets[ilod] = offset;
                lod_sides[ilod] = side + 1;
                offset += side * side + 6 * side + 5;
                side >>= 1;
            }

            lod_offsets[lods] = offset;
        } else {
            size_t offset = 0;
            size_t side = block_side;

            for (int ilod = 0; ilod < lods; ++ilod) {
                lod_offsets[ilod] = offset;
                lod_sides[ilod] = side + 1;
                offset += side * side;
                side >>= 1;
            }

            lod_offsets[lods] = offset;
        }
    }

    ~LODBufferSpecs() {
        if (lod_offsets) {
            memfree(lod_offsets);
        }

        if (lod_sides) {
            memfree(lod_sides);
        }
    }
};

template <typename T> class LODBufferPool;

// typedef uint16_t T;
template <typename T>
class LODBuffer {
    friend class MapStorage;
    friend class LODBufferPool<T>;

private:
    T *buffer;
    const LODBufferSpecs<T> specs;

public:
    LODBuffer(const LODBufferSpecs<T> &p_specs) : specs(p_specs)
    {
        size_t total_size = specs.page_size * specs.pages * sizeof(T);
        buffer = (T*)memalloc(total_size);
    }

    ~LODBuffer() {
        if (buffer) {
            memfree(buffer);
        }
    }

    // Non-copyable, non-movable
    LODBuffer(const LODBuffer&) = delete;
    LODBuffer& operator=(const LODBuffer&) = delete;
    LODBuffer(LODBuffer&&) = delete;
    LODBuffer& operator=(LODBuffer&&) = delete;

	_FORCE_INLINE_ const T *ptr() const { return buffer; }
    _FORCE_INLINE_ T *ptrw() { return buffer; }

    T *ptrw(size_t p_lod, size_t p_page, LODBUfferSection p_section = LODBUfferSection::Main) {
        ERR_FAIL_INDEX_V_EDMSG(p_lod, specs.lods, nullptr, "LOD out of range.");
        ERR_FAIL_INDEX_V_EDMSG(p_page, specs.pages, nullptr, "Page out of range.");
        size_t index = specs.lod_offsets[p_lod] + specs.page_size * p_page;

        if (p_section != LODBUfferSection::Main) {
            size_t side = specs.lod_sides[p_lod];
            index += side * (side + static_cast<int>(p_section));
        }

        return buffer + index;
    }

    void set(T p_value, size_t p_index, size_t p_lod, size_t p_page) {
        ERR_FAIL_INDEX_EDMSG(p_lod, specs.lods, "LOD out of range.");
        ERR_FAIL_INDEX_EDMSG(p_index, specs.lod_offsets[p_lod + 1] - specs.lod_offsets[p_lod], "Index out of range.");
        ERR_FAIL_INDEX_EDMSG(p_page, specs.pages, "Page out of range.");
        size_t index = p_index + specs.lod_offsets[p_lod] + specs.page_size * p_page;
        buffer[index] = p_value;
    }

    T get(size_t p_index, size_t p_lod, size_t p_page) const {
        ERR_FAIL_INDEX_EDMSG(p_lod, specs.lods, "LOD out of range.");
        ERR_FAIL_INDEX_EDMSG(p_index, specs.lod_offsets[p_lod + 1] - specs.lod_offsets[p_lod], "Index out of range.");
        ERR_FAIL_INDEX_EDMSG(p_page, specs.pages, "Page out of range.");
        size_t index = p_index + specs.lod_offsets[p_lod] + specs.page_size * p_page;
        return buffer[index];
    }

    void set_with_padding(T p_value, size_t p_x, size_t p_y, size_t p_lod, size_t p_page) {
        ERR_FAIL_COND_EDMSG(!specs.use_padding, "Buffer not using padding.");
        ERR_FAIL_INDEX_EDMSG(p_lod, specs.lods, "LOD out of range.");
        ERR_FAIL_INDEX_EDMSG(p_page, specs.pages, "Page out of range.");
        size_t side = specs.lod_sides[p_lod];
        ERR_FAIL_INDEX_EDMSG(p_x + 1, side + 2, "Index out of range.");
        ERR_FAIL_INDEX_EDMSG(p_y + 1, side + 2, "Index out of range.");
        size_t index = specs.lod_offsets[p_lod] + specs.page_size * p_page;

        if (p_x == -1) {
            ERR_FAIL_COND_EDMSG(p_y < 0 || p_y >= side, "Incorrect indices.");
            index += side * side + p_y;
        } else if (p_x == side) {
            ERR_FAIL_COND_EDMSG(p_y < 0 || p_y >= side, "Incorrect indices.");
            index += side * (side + 1) + p_y;
        } else if (p_y == -1) {
            index += side * (side + 2) + p_x;
        } else if (p_y == side) {
            index += side * (side + 3) + p_x;
        } else {
            index += p_x + p_y * side;
        }

        buffer[index] = p_value;
    }

    T get_with_padding(size_t p_x, size_t p_y, size_t p_lod, size_t p_page) const {
        ERR_FAIL_COND_EDMSG(!specs.use_padding, "Buffer not using padding.");
        ERR_FAIL_INDEX_EDMSG(p_lod, specs.lods, "LOD out of range.");
        ERR_FAIL_INDEX_EDMSG(p_page, specs.pages, "Page out of range.");
        size_t side = specs.lod_sides[p_lod];
        ERR_FAIL_INDEX_EDMSG(p_x + 1, side + 2, "Index out of range.");
        ERR_FAIL_INDEX_EDMSG(p_y + 1, side + 2, "Index out of range.");

        if (p_x == -1) {
            ERR_FAIL_COND_EDMSG(p_y < 0 || p_y >= side, "Incorrect indices.");
            size_t index = side * side + p_y;
            return buffer[index];
        } else if (p_x == side) {
            ERR_FAIL_COND_EDMSG(p_y < 0 || p_y >= side, "Incorrect indices.");
            size_t index = side * (side + 1) + p_y;
            return buffer[index];
        } else if (p_y == -1) {
            size_t index = side * (side + 2) + p_x;
            return buffer[index];
        } else if (p_y == side) {
            size_t index = side * (side + 3) + p_x;
            return buffer[index];
        } else {
            size_t index = p_x + p_y * side;
            return buffer[index];
        }
    }

    size_t size() const { return specs.page_size * specs.pages * sizeof(T); }
};

// typedef uint16_t T;
template <typename T>
class LODBufferPool {
    const size_t page_side;
    const LODBufferSpecs<T> specs;
    LODBuffer<T> **pool;
    size_t index = 0;

public:
    LODBufferPool(size_t p_block_side, size_t p_page_side, int p_lods, bool p_use_padding = false)
        : page_side(p_page_side), specs(LODBufferSpecs<T>(p_block_side, p_page_side * p_page_side, p_lods, p_use_padding))
    {
        pool = (LODBuffer<T>**)memalloc((p_page_side + 1) * sizeof(LODBuffer<T>));

        if (pool) {
            for (int i = 0; i <= page_side; ++i) {
                pool[i] = memnew(LODBuffer<T>(specs));
            }
        }

    }

    ~LODBufferPool() {
        if (pool) {
            for (int i = 0; i <= page_side; ++i) {
                memdelete(pool[i]);
            }

            memfree(pool);
        }
    }

    LODBuffer<T> *next() {
        size_t ret_index = index;
        index = (index + 1) % (page_side + 1);
        return pool[ret_index];
    }

    // LODBuffer<T> *get_prev_col(size_t p_row, size_t p_region_col, size_t p_lod) {
    //     if (p_region_col == 0) {
    //         LODBuffer<T> &buffer = *pool[index];
    //         buffer.pointer = buffer.ptrw(p_lod, p_row * page_side * specs.block_side * specs.block_side);
    //         return &buffer;
    //     } else {
    //         LODBuffer<T> &buffer = *pool[(index + page_side) % (page_side + 1)];
    //         buffer.pointer = buffer.ptrw(p_lod, (page_side - 1 + p_row * page_side) * specs.block_side * specs.block_side) + specs.block_side - 1;
    //         return &buffer;
    //     }
    // }

    // LODBuffer<T> *get_prev_row(size_t p_col, size_t p_region_row, size_t p_lod) {
    //     if (p_region_row == 0) {
    //         LODBuffer<T> &buffer = *pool[index];
    //         buffer.pointer = buffer.ptrw(p_lod, p_col * specs.block_side * specs.block_side);
    //         return &buffer;
    //     } else {
    //         LODBuffer<T> &buffer = *pool[(index + 1) % (page_side + 1)];
    //         // buffer.pointer = buffer.ptrw(p_lod, (p_col + (page_side - 1) * page_side) * specs.block_side * specs.block_side);
    //         return &buffer;
    //     }
    // }

    LODBuffer<T> *get_prev_region_col() {
        return pool[(index + page_side) % (page_side + 1)];
    }

    LODBuffer<T> *get_prev_region_row() {
        return pool[(index + 1) % (page_side + 1)];
    }

    LODBuffer<T> *get_prev_col(size_t p_region_col) {
        if (p_region_col == 0) {
            return pool[index];
        } else {
            return pool[(index + page_side) % (page_side + 1)];
        }
    }

    LODBuffer<T> *get_prev_row(size_t p_region_row) {
        if (p_region_row == 0) {
            return pool[index];
        } else {
            return pool[(index + 1) % (page_side + 1)];
        }
    }
};

} // namespace Terrainer

#endif // TERRAINER_LOD_BUFFER_H
