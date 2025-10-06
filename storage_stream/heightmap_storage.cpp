/**
 * heightmap_storage.cpp
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#include "heightmap_storage.h"

void THeightmapStorage::get_minmax(int p_lod_level, int p_x, int p_y, uint16_t &r_min_y, uint16_t &r_max_y) const {
    // TODO: Complete this!!!
    r_min_y = 0ui16;
    r_max_y = 0ui16;
}

THeightmapStorage::HeightmapBlock *THeightmapStorage::_create_block(Ref<FileAccess> p_file) const {
    HeightmapBlock *block = memnew(HeightmapBlock);
    block->file = p_file;
    HashMap<Vector2i, uint64_t> &map = block->chunk_positions;
    block->minmax_maps.resize(data.lod_level_count);
    Vector<MinMaxData> *minmax_ptr = block->minmax_maps.ptrw();
    minmax_ptr[0].resize(block_size * block_size);
    MinMaxData *minmax0 = minmax_ptr->ptrw();
    int index = 0;

    for (int iz = 0; iz < block_size; ++iz) {
        for (int ix = 0; ix < block_size; ++ix) {
            map[Vector2i(ix, iz)] = p_file->get_64();
            uint16_t min = p_file->get_16();
            uint8_t max = p_file->get_8();

            if (max > UINT16_MAX - min) [[unlikely]] {
                ERR_PRINT_ED("Overflow detected in minmax map.");
                minmax0[index] = MinMaxData(min, UINT16_MAX);
            } else {
                minmax0[index] = MinMaxData(min, min + max);
            }

            index++;
        }
    }

    for (int level = 1; level < data.lod_level_count; ++level) {
        const MinMaxData *src_minmax = block->minmax_maps.ptr()[level - 1].ptr();
        MinMaxData *dest_minmax = minmax_ptr[level].ptrw();
        int dest_size = block_size / (1 << level);
        minmax_ptr[level].resize(dest_size);
        index = 0;

        for (int iz = 0; iz < dest_size; ++iz) {
            for (int ix = 0; ix < dest_size; ++ix) {
                const MinMaxData &d00 = src_minmax[2 * ix + 4 * dest_size * iz];
                const MinMaxData &d10 = src_minmax[2 * ix + 1 + 4 * dest_size * iz];
                const MinMaxData &d01 = src_minmax[2 * ix + 2 * dest_size * (2 * iz + 1)];
                const MinMaxData &d11 = src_minmax[2 * ix + 1 + 2 * dest_size * (2 * iz + 1)];
                uint16_t min = MIN(d00.min, MIN(d01.min, MIN(d10.min, d11.min)));
                uint16_t max = MIN(d00.max, MIN(d01.max, MIN(d10.max, d11.max)));
                dest_minmax[index] = MinMaxData(min, max);
                index++;
            }
        }
    }

    return block;
}

THeightmapStorage::THeightmapStorage(TTerrainData &p_data) : data(p_data) {
}

THeightmapStorage::~THeightmapStorage() {
}