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

// #include <atomic>
// #include <cstdlib>
// #include <thread>
// #include <memory>

#include "../utils/math.h"
#include "core/io/file_access.h"

namespace Terrainer {

class Region {

    friend class MapStorage;

public:
    typedef uint16_t hmap_t;
    static const int MAX_LOD_LEVELS = 15;

private:
    static const size_t FILE_HEADER_INFO_SIZE = 64;
    static const size_t FILE_SPECS_SIZE = 32;
    static const size_t MAGIC_SIZE = 4;
    static constexpr char unsigned MAGIC_STRING[MAGIC_SIZE] = {'T', 'E', 'R', 'R'};
    static const uint8_t FORMAT_VERSION = 1ui8;
    static constexpr uint8_t REGION_FLAG_HAS_MINMAX = 1 << 0;
    static constexpr uint8_t REGION_FLAG_HAS_HMAP = 1 << 1;
    static constexpr uint8_t REGION_FLAG_HAS_SPLAT = 1 << 2;

    struct Specs {
        uint8_t version;
        uint8_t format;
        uint8_t region_lods: 4;
        uint8_t chunk_lods: 4;
        bool dirty = true;
        uint32_t chunk_size;
        uint32_t region_size;
        size_t region_buffer_size;
        size_t chunk_buffer_size;
        size_t chunk_padded_buffer_size;
        size_t hmap_buffer_size;

        void config(int p_chunk_lods = MAX_LOD_LEVELS) {
            ERR_FAIL_COND_EDMSG(p_chunk_lods < 0, "Chunk LODs must be positive.");
            region_lods = MIN((int)Math::log2(float(region_size)) + 1, MAX_LOD_LEVELS);
            chunk_lods = MIN(MIN((int)Math::log2(float(chunk_size)) + 1, MAX_LOD_LEVELS), p_chunk_lods);
            region_buffer_size = lod_geom_expand_sqr(region_size * region_size, region_lods);
            chunk_buffer_size = (chunk_size + 1) * (chunk_size + 1);
            chunk_padded_buffer_size = chunk_buffer_size + 4 * (chunk_size + 1);
            hmap_buffer_size = region_buffer_size * chunk_padded_buffer_size + lod_geom_expand_sqr(chunk_size * chunk_size, chunk_lods);
        }
    };

    struct alignas(FILE_HEADER_INFO_SIZE) HeaderInfo {
        char magic[MAGIC_SIZE];
        uint8_t version;
        uint8_t format;
        uint8_t region_lods: 4;
        uint8_t chunk_lods: 4;
        uint8_t u8_reserved;
        uint32_t chunk_size;
        uint32_t region_size;
    };
    static_assert(sizeof(HeaderInfo) == FILE_HEADER_INFO_SIZE);

    struct MinMax {
        hmap_t min;
        hmap_t max;
    };
    static_assert(sizeof(MinMax) == 2 * sizeof(hmap_t));

    const Ref<FileAccess> &access;
    const Specs &specs;
    uint8_t *buffer = nullptr;
    bool format_mismatch = false;
    MinMax *minmax_buffer = nullptr;
    hmap_t *hmap_buffer = nullptr;


public:
    bool load() {
        HeaderInfo info;
        access->get_buffer((uint8_t *)&info, FILE_HEADER_INFO_SIZE);

        for (int i = 0; i < MAGIC_SIZE; ++i) {
            ERR_FAIL_COND_V_EDMSG(info.magic[i] != MAGIC_STRING[i], false, vformat("Region file %s has incorrect format.", access->get_path().get_file()));
        }

        ERR_FAIL_COND_V_EDMSG(info.chunk_size != specs.chunk_size, false, vformat("Wrong chunk size in region file %s.", access->get_path().get_file()));
        ERR_FAIL_COND_V_EDMSG(info.region_size != specs.region_size, false, vformat("Wrong region size in region file %s.", access->get_path().get_file()));
        ERR_FAIL_COND_V_EDMSG(info.version > specs.version, false, vformat("Unsupported file version in region file %s.", access->get_path().get_file()));
        format_mismatch = info.format != specs.format || info.chunk_lods != specs.chunk_lods;
        size_t buffer_size = specs.region_buffer_size * sizeof(MinMax) + specs.hmap_buffer_size * sizeof(hmap_t);
        ERR_FAIL_COND_V_EDMSG(FileAccess::get_size(access->get_path_absolute()) != buffer_size + FILE_HEADER_INFO_SIZE, false, vformat("Incorrect file size for region file %s", access->get_path().get_file()));
        buffer = (uint8_t *)memalloc(buffer_size);
        access->get_buffer(buffer, buffer_size);
        minmax_buffer = (MinMax *)buffer;
        hmap_buffer = (hmap_t *)(buffer + specs.region_buffer_size * sizeof(MinMax));
    }

    Region(const Specs &p_specs, const Ref<FileAccess> &p_access)
        : specs(p_specs), access(p_access)
    { }

    ~Region() {
        if (buffer) {
            memfree(buffer);
            buffer = nullptr;
            minmax_buffer = nullptr;
            hmap_buffer = nullptr;
        }
    }
};

} // namespace Terrainer

#endif // TERRAINER_REGION_H

