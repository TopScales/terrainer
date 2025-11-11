/**
 * minmax_map.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_MINMAX_MAP_H
#define TERRAINER_MINMAX_MAP_H

#include "../terrain_info.h"
#include "core/io/file_access.h"

class TMinmaxMap {

private:
    Vector<PackedByteArray> maps;

    struct MinMax {
        uint16_t min = 0;
        uint16_t max = 0;

        MinMax(const uint8_t *p_data, bool p_is_lod0) {
            min = *p_data | (*(p_data + 1) << 8);

            if (p_is_lod0) {
                max = min + *(p_data + 2);
            } else {
                max = *(p_data + 2) | (*(p_data + 3) << 8);
            }
        }
    };

public:
    void setup(const TTerrainInfo &p_info, const TWorldInfo &p_world_info);
    void load_block(const Vector2i &p_block, const Ref<FileAccess> &p_file, const TWorldInfo &p_world_info, int p_max_lod);
    void generate_remaining_lods(int p_from_lod, const TWorldInfo &p_world_info);
    void get_minmax(uint16_t p_x, uint16_t p_z, int p_lod, const TWorldInfo &p_world_info, uint16_t &r_min, uint16_t &r_max);
    uint16_t get_chunk_min(uint16_t p_x, uint16_t p_z, const TWorldInfo &p_world_info);
    void clear();

    TMinmaxMap();
    ~TMinmaxMap();
};

#endif // TERRAINER_MINMAX_MAP_H