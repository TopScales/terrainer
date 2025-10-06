/**
 * terrain_info.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_TERRAIN_INFO_H
#define TERRAINER_TERRAIN_INFO_H

#include "core/variant/variant.h"

// class THeightmapStorage;

struct TTerrainInfo {
    // World.
    int chunk_size = 16;
    Vector3 map_scale = Vector3(1.0, 1.0, 1.0);
    int block_size = 8;
    Vector2i world_blocks = Vector2i(4, 4);

    // LOD.
    int lod_levels = 0;
    real_t lod_distance_ratio = 2.0;

    bool include_all_nodes_in_range = false;
    Vector<Plane> frustum;
    uint16_t root_node_size = 16;
    uint16_t root_nodes_count_x = 4;
    uint16_t root_nodes_count_z = 4;
    int min_selected_lod = 1;
    int max_selected_lod = 1;

    // static const int MIN_QUAD_TREE_LEAF_NODE_SIZE = 4;
//     Mesh Grid Size - LeafQTNodeSz - pw(2)
//   - Mesh Scale - V3
//   - LOD Detailed Chunks
    // World.
	// Vector2i raster_size;
	// AABB map_dimensions;

    // LOD settings.
	// int qt_leaf_node_size = 8;
    // int render_grid_resolution_multiplier = 1;
    // int lod_level_count = 5;
    // real_t lod_distance_ratio = 2.0;
    // int mesh_size = 8;

    // Ref<THeightmapStorage> heightmap;
    // int heightmap_lod_offset = 3;
    // int heightmap_block_size = 512;
    // bool detail_heightmap_enabled = false;

    // int terrain_mesh_size = 32;
};

#endif // TERRAINER_TERRAIN_INFO_H