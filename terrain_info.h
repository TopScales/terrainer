/**
 * terrain_info.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_TERRAIN_INFO_H
#define TERRAINER_TERRAIN_INFO_H

#ifdef TERRAINER_MODULE
#include "core/variant/variant.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/variant/builtin_types.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;
#endif // TERRAINER_GDEXTENSION

struct TWorldInfo {
    int chunk_size = 16;
    int block_size = 32;
    Vector3 map_scale = Vector3(1.0, 1.0, 1.0);
    Vector2i world_blocks = Vector2i(4, 4);

    _FORCE_INLINE_ Vector3 get_max_world_size() const {
        int block_cells = block_size * chunk_size;
        return Vector3(world_blocks.x * block_cells * map_scale.x, UINT16_MAX * map_scale.y, world_blocks.y * block_cells * map_scale.z);
    }
};

struct TTerrainInfo {
    // LOD.
    int lod_levels = 0;
    real_t lod_distance_ratio = 2.0;

    bool include_all_nodes_in_range = false;
#ifdef TERRAINER_MODULE
    Vector<Plane> frustum;
#elif TERRAINER_GDEXTENSION
    TypedArray<Plane> frustum;
#endif
    uint16_t root_node_size = 1;
    uint16_t root_nodes_count_x = 1;
    uint16_t root_nodes_count_z = 1;
    int min_selected_lod = 1;
    int max_selected_lod = 1;
};

#endif // TERRAINER_TERRAIN_INFO_H