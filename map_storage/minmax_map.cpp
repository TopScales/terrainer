/**
 * minmax_map.cpp
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */
#include "minmax_map.h"

#include "../utils/compat_marshalls.h"
#include "map_storage.h"

void TMinmaxMap::setup(const TTerrainInfo &p_info) {
    maps.resize(p_info.lod_levels);
    int size = p_info.block_size * p_info.block_size * p_info.world_blocks.x * p_info.world_blocks.y;

    for (int i = 0; i < p_info.lod_levels; ++i) {
        const int sector_size = i == 0 ? 3 : 4;
        maps.get(i).resize(size * sector_size);
        size >>= 1;
    }
}

void TMinmaxMap::load_block(const Vector2i &p_block, const Ref<FileAccess> &p_file, const TTerrainInfo &p_info, int p_max_lod) {
    p_file->seek(TMapStorage::HEADER_SIZE);
    int length = p_info.block_size;
    const int x = p_block.x + p_info.world_blocks.x / 2;
    const int z = p_block.y + p_info.world_blocks.y / 2;

    for (int ilod = 0; ilod < p_max_lod; ++ilod) {
        const int block_offset = length * (x + z * length * p_info.world_blocks.x);
        uint8_t *buffer = maps.get(ilod).ptrw();
        const int segmet_size = ilod == 0 ? 3 : 4;

        for (int irow = 0; irow < length; ++irow) {
            const PackedByteArray row = p_file->get_buffer(length * segmet_size);
            const int offset = block_offset + irow * length * p_info.world_blocks.x;
            memcpy(buffer + offset, row.ptr(), length * segmet_size);
        }

        length >>= 1;
    }
}

void TMinmaxMap::generate_remaining_lods(int p_from_lod, const TTerrainInfo &p_info) {
    // TODO: Check last lod is within bounds.
    ERR_FAIL_INDEX(p_from_lod, maps.size());
    const int next_lod = p_from_lod + 1;
    int size_x = (p_info.block_size * p_info.world_blocks.x) >> next_lod;
    int size_z = (p_info.block_size * p_info.world_blocks.y) >> next_lod;

    for (int ilod = next_lod; ilod < maps.size(); ++ilod) {
        DEV_ASSERT(size_x * size_z * 4 == maps[ilod].size());
        const int src_lod = ilod - 1;
        const bool is_src_lod0 = src_lod == 0;
        const uint8_t *src = maps.get(src_lod).ptr();
        uint8_t *dst = maps.get(ilod).ptrw();
        const int src_step_size = src_lod == 0 ? 3 : 4;
        const int src_size_x = 2 * size_x * src_step_size;
        int idx = 0;

        for (int iz = 0; iz < size_z; ++iz) {
            for (int ix = 0; ix < size_x; ++ix) {
                const uint8_t *src_ptr = src + 2 * idx * src_step_size;
                MinMax src_minmax_00 = MinMax(src_ptr, is_src_lod0);
                MinMax src_minmax_10 = MinMax(src_ptr + src_step_size, is_src_lod0);
                MinMax src_minmax_01 = MinMax(src_ptr + src_size_x, is_src_lod0);
                MinMax src_minmax_11 = MinMax(src_ptr + src_size_x + src_step_size, is_src_lod0);
                uint16_t hmin = MIN(src_minmax_00.min, MIN(src_minmax_10.min, MIN(src_minmax_01.min, src_minmax_11.min)));
                uint16_t hmax = MAX(src_minmax_00.max, MAX(src_minmax_10.max, MAX(src_minmax_01.max, src_minmax_11.max)));
                uint8_t *dst_ptr = dst + idx * 4;
                encode_uint16(hmin, dst_ptr);
                encode_uint16(hmax, dst_ptr + 2);
                idx++;
            }
        }

        size_x >>= 1;
        size_z >>= 1;
    }
}

void TMinmaxMap::get_minmax(uint16_t p_x, uint16_t p_z, int p_lod, const TTerrainInfo &p_info, uint16_t &r_min, uint16_t &r_max) {
    const bool is_lod0 = p_lod == 0;
    const int step_size = is_lod0 ? 3 : 4;
    const int size = (p_info.block_size * p_info.world_blocks.x) >> p_lod;
    const int index = (p_x + p_z * size) * step_size;
    ERR_FAIL_COND_EDMSG(index + step_size > maps.get(p_lod).size(), vformat("Minmax map indices (%d, %d) out of bounds for LOD %d.", p_x, p_z, p_lod));
    const uint8_t *map = maps.get(p_lod).ptr();
    const MinMax minmax = MinMax(map + index, is_lod0);
    r_min = minmax.min;
    r_max = minmax.max;
}

uint16_t TMinmaxMap::get_chunk_min(uint16_t p_x, uint16_t p_z, const TTerrainInfo &p_info) {
    const int size = p_info.block_size * p_info.world_blocks.x;
    const int index = (p_x + p_z * size) * 3;
    ERR_FAIL_COND_EDMSG(index + 3 > maps.get(0).size(), vformat("Minmax map indices (%d, %d) out of bounds for LOD 0."));
    const uint8_t *map = maps.get(0).ptr() + index;
    uint16_t min = *map | (*(map + 1) << 8);
    return min;
}

void TMinmaxMap::clear() {
    for (int i = 0; i < maps.size(); ++i) {
        maps.get(i).clear();
    }

    maps.clear();
}

TMinmaxMap::TMinmaxMap() {
}

TMinmaxMap::~TMinmaxMap() {
    clear();
}
