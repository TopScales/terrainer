/**
 * region.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_REGION_H
#define TERRAINER_REGION_H

#include "../utils/math.h"
#include "core/io/file_access.h"

namespace Terrainer {

class Region {

    friend class Sector;
    friend class MapStorage;

public:
    typedef uint16_t hmap_t;
    static const int MAX_LOD_LEVELS = 15;

    struct CellKey {
        uint16_t x;
        uint16_t z;

        constexpr CellKey() : x(0), z(0) {}
        constexpr CellKey(uint16_t p_x, uint16_t p_z) : x(p_x), z(p_z) {}
        constexpr CellKey(const Vector2i &p_in) : x(p_in.x), z(p_in.y) {}

        constexpr CellKey operator+(CellKey p_k) const { return CellKey(x + p_k.x, z + p_k.z); }
        constexpr void operator+=(CellKey p_k) { x += p_k.x; z += p_k.z; }
        constexpr CellKey operator-(CellKey p_k) const { return CellKey(x - p_k.x, z - p_k.z); }
        constexpr void operator-=(CellKey p_k) { x -= p_k.x; z -= p_k.z; }
        constexpr CellKey operator*(CellKey p_k) const { return CellKey(x * p_k.x, z * p_k.z); }
        constexpr CellKey operator*(uint16_t p_k) const { return CellKey(x * p_k, z * p_k); }
        constexpr CellKey operator/(uint16_t p_k) const { return CellKey(x / p_k, z / p_k); }
        constexpr bool operator==(CellKey p_k) const { return x == p_k.x && z == p_k.z; }
        constexpr bool operator!=(CellKey p_k) const { return x != p_k.x || z != p_k.z; }

        _FORCE_INLINE_ Vector3 position(real_t p_scale_x, real_t p_scale_z) const {
            return Vector3(x * p_scale_x, 0.0, z * p_scale_z);
        }

        uint32_t hash() const {
            return hash_murmur3_one_32((uint32_t)x | ((uint32_t)z << 16));
	    }
    };
    static_assert(sizeof(CellKey) == 4);

private:
    static const size_t FILE_HEADER_INFO_SIZE = 64;
    static const size_t FILE_SPECS_SIZE = 32;
    static const size_t MAGIC_SIZE = 4;
    static constexpr char unsigned MAGIC_STRING[MAGIC_SIZE] = {'T', 'E', 'R', 'R'};
    static const uint8_t FORMAT_VERSION = 1ui8;
    static constexpr uint8_t REGION_FLAG_HAS_MINMAX = 1 << 0;
    static constexpr uint8_t REGION_FLAG_HAS_HMAP = 1 << 1;
    static constexpr uint8_t REGION_FLAG_HAS_SPLAT = 1 << 2;

    enum class ChunkPad {
        X_NEG,
        X_POS,
        Z_NEG,
        Z_POS
    };

    struct MinMax {
        hmap_t min;
        hmap_t max;

        constexpr MinMax() : min(0), max(0) {}
        constexpr MinMax(hmap_t p_min, hmap_t p_max) : min(p_min), max(p_max) {}
    };
    static_assert(sizeof(MinMax) == 2 * sizeof(hmap_t));

    struct Specs {
        uint8_t version = 0;
        uint8_t format = 0;
        uint8_t region_lods: 4;
        uint8_t chunk_lods: 4;
        bool dirty = true;
        uint16_t chunk_size = 32ui16;
        uint16_t region_size = 32ui16;
        size_t region_buffer_size = 0;
        size_t hmap_buffer_size = 0;
        size_t *minmax_lod_offsets = nullptr;
        size_t *hmap_lod_offsets = nullptr;
        size_t *hmap_lod_chunk_sizes = nullptr;

        size_t *sector_minmax_lod_offsets = nullptr;
        uint16_t sector_size = 0ui16; // In terms of chunks.
        int lods = 0;
        int sector_regions = 0;
        size_t sector_minmax_buffer_size = 0;

        hmap_t default_height = 0;
        MinMax default_minmax;

        void config(int p_chunk_lods = MAX_LOD_LEVELS) {
            ERR_FAIL_COND_EDMSG(p_chunk_lods < 0, "Chunk LODs must be positive.");
            region_lods = MIN((int)Math::log2(float(region_size)) + 1, MAX_LOD_LEVELS);
            chunk_lods = MIN(MIN((int)Math::log2(float(chunk_size)) + 1, MAX_LOD_LEVELS), p_chunk_lods);
            const size_t chunk_padded_buffer_size = (chunk_size + 1) * (chunk_size + 1) + 4 * (chunk_size + 1);

            if (minmax_lod_offsets) {
                memfree(minmax_lod_offsets);
            }

            if (hmap_lod_offsets) {
                memfree(hmap_lod_offsets);
            }

            if (hmap_lod_chunk_sizes) {
                memfree(hmap_lod_chunk_sizes);
            }

            minmax_lod_offsets = (size_t *)memalloc((region_lods + 1) * sizeof(size_t));
            hmap_lod_offsets = (size_t *)memalloc((region_lods + chunk_lods + 1) * sizeof(size_t));
            hmap_lod_chunk_sizes = (size_t *)memalloc((region_lods + chunk_lods) * sizeof(size_t));
            size_t minmax_offset = 0;
            size_t hmap_offset = 0;
            size_t side = region_size;

            for (int ilod = 0; ilod < region_lods; ++ilod) {
                minmax_lod_offsets[ilod] = minmax_offset;
                hmap_lod_offsets[ilod] = hmap_offset;
                hmap_lod_chunk_sizes[ilod] = chunk_padded_buffer_size;
                minmax_offset += side * side;
                hmap_offset += chunk_padded_buffer_size * side * side;
                side >>= 1;
            }

            side = chunk_size >> 1;

            for (int ilod = region_lods; ilod < region_lods + chunk_lods; ++ilod) {
                hmap_lod_offsets[ilod] = hmap_offset;
                hmap_lod_chunk_sizes[ilod] = side * side;
                hmap_offset += side * side;
                side >>= 1;
            }

            minmax_lod_offsets[region_lods] = minmax_offset;
            hmap_lod_offsets[region_lods + chunk_lods] = hmap_offset;
            region_buffer_size = minmax_offset;
            hmap_buffer_size = hmap_offset;
            dirty = false;
        }

        void set_sector_info(uint16_t p_sector_size, int p_lods) {
            sector_size = p_sector_size;
            lods = p_lods;
            sector_regions = MAX(sector_size / region_size, 1);

            if (sector_minmax_lod_offsets) {
                memfree(sector_minmax_lod_offsets);
                sector_minmax_lod_offsets = nullptr;
            }

            if (lods > region_lods) {
                int extra_lods = lods - region_lods;
                sector_minmax_lod_offsets = (size_t *)memalloc((extra_lods + 1) * sizeof(size_t));
                size_t minmax_offset = 0;
                size_t side = sector_regions >> 1;

                for (int ilod = 0; ilod < extra_lods; ++ilod) {
                    sector_minmax_lod_offsets[ilod] = minmax_offset;
                    minmax_offset += side * side;
                    side >>= 1;
                }

                sector_minmax_lod_offsets[extra_lods] = minmax_offset;
                sector_minmax_buffer_size = minmax_offset;
            }
        }

        _FORCE_INLINE_ size_t get_buffer_size() const { return region_buffer_size * sizeof(MinMax) + hmap_buffer_size * sizeof(hmap_t); }
        _FORCE_INLINE_ size_t get_minmax_buffer_size() const { return region_buffer_size; }

        ~Specs() {
            if (minmax_lod_offsets) {
                memfree(minmax_lod_offsets);
            }

            if (hmap_lod_offsets) {
                memfree(hmap_lod_offsets);
            }

            if (hmap_lod_chunk_sizes) {
                memfree(hmap_lod_chunk_sizes);
            }

            if (sector_minmax_lod_offsets) {
                memfree(sector_minmax_lod_offsets);
            }
        }
    };

    struct alignas(FILE_HEADER_INFO_SIZE) HeaderInfo {
        char magic[MAGIC_SIZE];
        uint8_t version;
        uint8_t format;
        uint8_t region_lods: 4;
        uint8_t chunk_lods: 4;
        uint8_t u8_reserved = 0;
        uint32_t chunk_size;
        uint32_t region_size;
    };
    static_assert(sizeof(HeaderInfo) == FILE_HEADER_INFO_SIZE);

    const Ref<FileAccess> access;
    const Specs &specs;
    uint8_t *buffer = nullptr;
    bool format_mismatch = false;
    MinMax *minmax_buffer = nullptr;
    hmap_t *hmap_buffer = nullptr;

    void write_header() const;
    _FORCE_INLINE_ hmap_t *get_hmap_chunk(size_t p_lod, size_t p_chunk_idx) const;
    _FORCE_INLINE_ hmap_t *get_hmap_chunk_pad(size_t p_lod, size_t p_chunk_idx, ChunkPad p_pad) const;
    _FORCE_INLINE_ MinMax get_minmax(size_t p_lod, size_t p_node_idx) const {
#ifdef TERRAINER_DEBUG
        ERR_FAIL_INDEX_V_EDMSG(p_lod, specs.region_lods, MinMax(specs.default_height, specs.default_height + 1), "LOD out of range.");
        ERR_FAIL_INDEX_V_EDMSG(p_node_idx, specs.minmax_lod_offsets[p_lod + 1] - specs.minmax_lod_offsets[p_lod], MinMax(specs.default_height, specs.default_height + 1), "Block out of range.");
#endif
        return *(minmax_buffer + specs.minmax_lod_offsets[p_lod] + p_node_idx);
    }

public:
    bool load();
    void load_hmap_region(const CellKey &p_region, const CellKey &p_regions, const PackedByteArray &p_data, const Vector2i &p_size);
    void fill_hmap_region_pad(const CellKey &p_region, const CellKey &p_regions, Vector<Region *> &p_regions_pool, int p_pool_index);
    void store_hmap() const;
    PackedInt32Array get_hmap_chunk_values(size_t p_lod, const CellKey &p_chunk) const;

    Region(const Specs &p_specs, const Ref<FileAccess> &p_access);
    ~Region();
};

} // namespace Terrainer

#endif // TERRAINER_REGION_H

