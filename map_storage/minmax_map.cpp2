/**
 * minmax_map.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */
#include "minmax_map.h"

#include "map_storage.h"
#include "../utils/compat_marshalls.h"

void TMinmaxMap::setup(const TTerrainInfo &p_info, const TWorldInfo &p_world_info) {
    maps.clear();
    int size = p_world_info.block_size * p_world_info.block_size * p_world_info.world_blocks.x * p_world_info.world_blocks.y;
    minmax_section_size = 3 * size + int(8.0 * size * (1.0 - 1.0 / Math::pow(4.0, minmax_saved_lods)) / 3.0);

    for (int i = 0; i < p_info.lod_levels; ++i) {
        const int sector_size = i == 0 ? 3 : 4;
        PackedByteArray map;
        int map_size = size * sector_size;
        map.resize(size * sector_size);
        maps.push_back(map);
        size >>= 1;
    }
}

void TMinmaxMap::load_block(const Vector2i &p_block, const Ref<FileAccess> &p_file, const TWorldInfo &p_world_info) {
    p_file->seek(TMapStorage::HEADER_SIZE);
    int length = p_world_info.block_size;
    const int x = p_block.x + p_world_info.world_blocks.x / 2;
    const int z = p_block.y + p_world_info.world_blocks.y / 2;

    for (int ilod = 0; ilod < minmax_saved_lods; ++ilod) {
        const int block_offset = length * (x + z * length * p_world_info.world_blocks.x);
        uint8_t *buffer = maps.get(ilod).ptrw();
        const int segmet_size = ilod == 0 ? 3 : 4;

        for (int irow = 0; irow < length; ++irow) {
            const PackedByteArray row = p_file->get_buffer(length * segmet_size);
            const int offset = block_offset + irow * length * p_world_info.world_blocks.x; // FIX: Need to consider segement size?
            memcpy(buffer + offset, row.ptr(), length * segmet_size);
        }

        length >>= 1;
    }
}

void TMinmaxMap::fill_block(const Vector2i &p_block, uint16_t p_value, const TWorldInfo &p_world_info) {
    int length = p_world_info.block_size;
    const int x = p_block.x + p_world_info.world_blocks.x / 2;
    const int z = p_block.y + p_world_info.world_blocks.y / 2;
    Vector<uint8_t> value;
    value.resize(4);
    uint8_t *value_ptr = value.ptrw();

    for (int ilod = 0; ilod < minmax_saved_lods; ++ilod) {
        const int block_offset = length * (x + z * length * p_world_info.world_blocks.x);
        uint8_t *buffer = maps.get(ilod).ptrw();
        const int segmet_size = ilod == 0 ? 3 : 4;
        encode_uint16(p_value, value_ptr);
        encode_uint16(ilod == 0 ? 0ui16 : p_value, value_ptr + 2);

        for (int irow = 0; irow < length; ++irow) {
            const int offset = block_offset + irow * length * p_world_info.world_blocks.x;

            for (int icol = 0; icol < length; ++icol) {
                memcpy(buffer + offset + icol * segmet_size, value_ptr, segmet_size);
            }
        }

        length >>= 1;
    }
}

void TMinmaxMap::generate_remaining_lods(const TWorldInfo &p_world_info) {
    const int next_lod = minmax_saved_lods + 1;
    int size_x = (p_world_info.block_size * p_world_info.world_blocks.x) >> next_lod;
    int size_z = (p_world_info.block_size * p_world_info.world_blocks.y) >> next_lod;

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
                const uint8_t *src_ptr = src + (2 * ix + 4 * iz * size_x) * src_step_size;
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

void TMinmaxMap::get_minmax(uint16_t p_x, uint16_t p_z, int p_lod, const TWorldInfo &p_world_info, uint16_t &r_min, uint16_t &r_max) const {
    const bool is_lod0 = p_lod == 0;
    const int step_size = is_lod0 ? 3 : 4;
    const int size = (p_world_info.block_size * p_world_info.world_blocks.x) >> p_lod;
    const int index = (p_x + p_z * size) * step_size;
    ERR_FAIL_COND_EDMSG(index + step_size > maps.get(p_lod).size(), vformat("Minmax map indices (%d, %d) out of bounds for LOD %d.", p_x, p_z, p_lod));
    const uint8_t *map = maps.get(p_lod).ptr();
    const MinMax minmax = MinMax(map + index, is_lod0);
    r_min = minmax.min;
    r_max = MAX(minmax.max, r_min + 1ui16);
}

uint16_t TMinmaxMap::get_chunk_min(uint16_t p_x, uint16_t p_z, const TWorldInfo &p_world_info) {
    const int size = p_world_info.block_size * p_world_info.world_blocks.x;
    const int index = (p_x + p_z * size) * 3;
    ERR_FAIL_COND_V_EDMSG(index + 3 > maps.get(0).size(), 0ui16, vformat("Minmax map indices (%d, %d) out of bounds for LOD 0."));
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

void TMinmaxMap::set_saved_lods(int p_lods) {
    minmax_saved_lods = p_lods;
}

int TMinmaxMap::get_saved_lods() const {
    return minmax_saved_lods;
}

int TMinmaxMap::get_section_size() const {
    return minmax_section_size;
}

TMinmaxMap::TMinmaxMap() {
}

TMinmaxMap::~TMinmaxMap() {
    clear();
}
