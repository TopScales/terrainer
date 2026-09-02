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

struct LODBufferSpecs {
    template <typename U>
    friend class LODBuffer;

    friend class MapStorage;

private:
    size_t block_side = 0;
    size_t pages = 0;
    int lods = 0;
    bool use_padding = false;
    size_t page_size = 0;
    size_t* lod_offsets = nullptr;
    size_t* lod_sides = nullptr;
    bool dirty = true;

public:
    void config(size_t p_block_side, size_t p_pages, int p_lods, bool p_use_padding = false) {
        if (lod_offsets) {
            memfree(lod_offsets);
        }

        if (lod_sides) {
            memfree(lod_sides);
        }

        block_side = p_block_side;
        pages = p_pages;
        lods = p_lods;
        use_padding = p_use_padding;
        page_size = use_padding ? lod_geom_expand_sqr(block_side * block_side, lods) + lod_geom_expand(6 * block_side, lods) + 5 * lods
            : lod_geom_expand_sqr(block_side * block_side, lods);
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

        dirty = false;
    }

    void set_dirty() { dirty = true; }
    bool is_dirty() { return dirty; }

    LODBufferSpecs() {}

    ~LODBufferSpecs() {
        if (lod_offsets) {
            memfree(lod_offsets);
        }

        if (lod_sides) {
            memfree(lod_sides);
        }
    }
};

// typedef uint16_t T;
template <typename T>
class LODBuffer {
    friend class MapStorage;

private:
    T *buffer = nullptr;
    const LODBufferSpecs &specs;

public:
    LODBuffer(const LODBufferSpecs &p_specs) : specs(p_specs)
    {
        size_t total_size = specs.page_size * specs.pages * sizeof(T);
        buffer = (T*)memalloc(total_size);
    }

    ~LODBuffer() {
        if (buffer) {
            memfree(buffer);
            buffer = nullptr;
        }
    }

    // Non-copyable, non-movable
    LODBuffer(const LODBuffer&) = delete;
    LODBuffer& operator=(const LODBuffer&) = delete;
    LODBuffer(LODBuffer&&) = delete;
    LODBuffer& operator=(LODBuffer&&) = delete;

	_FORCE_INLINE_ const T *ptr() const { return buffer; }
    _FORCE_INLINE_ T *ptrw() { return buffer; }
    _FORCE_INLINE_ const uint8_t *bytes() const { return reinterpret_cast<uint8_t *>(buffer); }
    _FORCE_INLINE_ uint8_t *bytesw() const { return reinterpret_cast<uint8_t *>(buffer); }

    const T *ptr(size_t p_lod, size_t p_page, LODBUfferSection p_section = LODBUfferSection::Main) const {
        ERR_FAIL_INDEX_V_EDMSG(p_lod, specs.lods, nullptr, "LOD out of range.");
        ERR_FAIL_INDEX_V_EDMSG(p_page, specs.pages, nullptr, "Page out of range.");
        size_t index = specs.lod_offsets[p_lod] + specs.page_size * p_page;

        if (p_section != LODBUfferSection::Main) {
            size_t side = specs.lod_sides[p_lod];
            index += side * (side + static_cast<int>(p_section));
        }

        return buffer + index;
    }

    T *ptrw(size_t p_lod, size_t p_page, LODBUfferSection p_section = LODBUfferSection::Main) {
        ERR_FAIL_INDEX_V_EDMSG(p_lod, specs.lods, nullptr, vformat("LOD out of range (Requested: %d; Max: %d).", p_lod, specs.lods - 1));
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

    Vector<uint8_t> to_byte_array() const {
		Vector<uint8_t> ret;
        size_t total_size = specs.page_size * specs.pages * sizeof(T);
        ret.resize(total_size);

        if (total_size) {
            memcpy(ret.ptrw(), ptr(), total_size);
        }

        return ret;
	}
};

} // namespace Terrainer

#endif // TERRAINER_LOD_BUFFER_H
