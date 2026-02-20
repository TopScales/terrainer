/**
 * lod_quad_tree.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#include "lod_quad_tree.h"

#include "utils/compat_marshalls.h"
#include "utils/math.h"

using namespace Terrainer;

void LODQuadTree::set_map_info(int p_chunk_size, int p_region_size, const Vector2i p_world_regions, const Vector3 &p_map_scale) {
    chunk_size = p_chunk_size;
    region_size = p_region_size;
    world_size = p_world_regions * region_size;
    map_scale = p_map_scale;
}

void LODQuadTree::set_lod_levels(real_t p_far_view, int p_lod_detailed_chunks_radius) {
    lod_levels = 1;
    const real_t radius0 = LOD0_RADIUS_FACTOR * p_lod_detailed_chunks_radius * chunk_size * MAX(map_scale.x, map_scale.z);
    real_t level_radius = radius0;
    real_t current_radius = 0.0;
    real_t next_radius = level_radius;
    sector_size = 1;
    const int min_world_size = MIN(world_size.x, world_size.y);
    int num_nodes = 0; // Estimate number of nodes.
    real_t node_size = chunk_size * MIN(map_scale.x, map_scale.z); // Size used for estimation.

    while (next_radius < p_far_view && sector_size < min_world_size && lod_levels <= MapStorage::MAX_LOD_LEVELS) {
        int n = int(Math::ceil(2 * next_radius / node_size));
        int inner = int(Math::floor(2 * current_radius / node_size));
        num_nodes += n * n - inner * inner;
        current_radius = next_radius;
        level_radius *= lod_distance_ratio;
        next_radius = level_radius + current_radius;
        sector_size *= 2;
        node_size *= 2.0;
        lod_levels++;
    }

    print_line(vformat("Number of total estimated nodes: %d", num_nodes));

    if (current_radius + level_radius - p_far_view > 0.5 * level_radius) {
        lod_levels--;
        sector_size /= 2;
    }

    lod_visibility_range.resize(lod_levels);
    lod_visibility_range.set(lod_levels - 1, p_far_view);
    level_radius = radius0;
    current_radius = radius0;

    for (int i = 0; i < lod_levels - 1; ++i) {
        lod_visibility_range.set(i, current_radius);
        level_radius *= lod_distance_ratio;
        current_radius += level_radius;
    }

    sector_count_x = Math::ceil((real_t)world_size.x / (real_t)sector_size);
    sector_count_z = Math::ceil((real_t)world_size.y / (real_t)sector_size);
    lods_count.resize(lod_levels);
    real_t offset_x = (real_t)(world_size.x / 2) * chunk_size * map_scale.x;
    real_t offset_z = (real_t)(world_size.y / 2) * chunk_size * map_scale.z;
    world_offset = Vector3(-offset_x, 0.0, -offset_z);
}

LODQuadTree::NodeSelectionResult LODQuadTree::select_sector_nodes(const Vector3 &p_viewer_position, CellKey p_sector, const Ref<MapStorage> &p_storage, int p_stop_at_lod_level) {
    if (p_sector.cell.x >= sector_count_x || p_sector.cell.z >= sector_count_z) {
        return OutOfMap;
    }

    return _lod_select(p_viewer_position, p_storage, false, NodeKey(p_sector, CellKey()), sector_size, lod_levels - 1, 0);
}

void LODQuadTree::update_stats() {
    int *count_ptr = lods_count.ptrw();
    // int min_selected_lod = MapStorage::MAX_LOD_LEVELS;
    // int max_selected_lod = 0;

    for (int i = 0; i < selection_count; ++i) {
        int selected_node_lod_level = selected_buffer[i].get_lod_level();
        // min_selected_lod = MIN(min_selected_lod, selected_node_lod_level);
        // max_selected_lod = MAX(max_selected_lod, selected_node_lod_level);
        count_ptr[selected_node_lod_level]++;
    }
}

// void LODQuadTree::select_nodes(const Vector3 &p_viewer_position, const TMinmaxMap &p_minmax_map, int p_stop_at_lod_level) {
//     selection_count = 0;
//     // TODO: Organize root nodes by lod range.
//     // TODO: Load textures without frustum culling.

//     for (uint16_t iz = 0; iz < info->root_nodes_count_z; ++iz) {
//         for (uint16_t ix = 0; ix < info->root_nodes_count_x; ++ix) {
//             _lod_select(p_viewer_position, p_minmax_map, false, ix, iz, info->root_node_size, info->lod_levels - 1, 0);
//         }
//     }

//     info->min_selected_lod = MAX_LOD_LEVELS;
//     info->max_selected_lod = 0;
//     lods_count.fill(0);
//     int *count_ptr = lods_count.ptrw();

//     for (int i = 0; i < selection_count; ++i) {
//         int selected_node_lod_level = selected_buffer[i].get_lod_level();
//         info->min_selected_lod = MIN(info->min_selected_lod, selected_node_lod_level);
//         info->max_selected_lod = MAX(info->max_selected_lod, selected_node_lod_level);
//         count_ptr[selected_node_lod_level]++;
//     }
// }

// int LODQuadTree::get_selection_count() const {
//     return selection_count;
// }

const LODQuadTree::QTNode *LODQuadTree::get_selected_node(int p_index) const {
    ERR_FAIL_INDEX_V_EDMSG(p_index, selection_count, nullptr, "Selected node index out of bounds.");
    return &selected_buffer[p_index];
}

// AABB LODQuadTree::get_selected_node_aabb(int p_index) const {
//     ERR_FAIL_INDEX_V_EDMSG(p_index, selection_count, AABB(), "Selected node index out of bounds.");
//     const QTNode &node = selected_buffer[p_index];
//     Vector3 size = Vector3(node.size, node.max_y - node.min_y, node.size) * world_info->map_scale;
//     Vector3 position = Vector3(node.x * size.x, node.min_y * world_info->map_scale.y, node.z * size.z);
//     return AABB(position, size);
// }

// int LODQuadTree::get_selected_node_lod(int p_index) const {
//     ERR_FAIL_INDEX_V_EDMSG(p_index, selection_count, 0, "Selected node index out of bounds.");
//     const QTNode &node = selected_buffer[p_index];
//     return node.get_lod_level();
// }

int LODQuadTree::get_lod_nodes_count(int p_level) const {
    ERR_FAIL_INDEX_V_EDMSG(p_level, lods_count.size(), 0, "Invalid LOD level index.");
    return lods_count[p_level];
}

// Ref<ImageTexture> LODQuadTree::get_morph_texture(real_t p_morph_start_ratio) const {
//     PackedByteArray buffer;
//     buffer.resize(4 * info->lod_levels);
//     real_t prev_pos = 0.0;
//     uint8_t *w = buffer.ptrw();

//     for (int i = 0; i < info->lod_levels; ++i) {
//         float end = lod_visibility_range[i];
//         float start = prev_pos + (end - prev_pos) * p_morph_start_ratio;
//         float c1 = end / (end - start);
//         float c2 = 1.0f / (end - start);
//         int64_t index = 4 * i;
// 		encode_uint16(MAKE_HALF_FLOAT(c1), &w[index]);
//         encode_uint16(MAKE_HALF_FLOAT(c2), &w[index + 2]);
//         prev_pos = end;
//     }

//     Ref<Image> image = Image::create_from_data(info->lod_levels, 1, false, Image::FORMAT_RGH, buffer);
//     Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
//     return texture;
// }

Transform3D LODQuadTree::get_node_transform(const QTNode *p_node) const {
    const Vector3 bx = Vector3(p_node->size * map_scale.x, 0.0, 0.0);
    const Vector3 by = Vector3(0.0, 1.0, 0.0);
    const Vector3 bz = Vector3(0.0, 0.0, p_node->size * map_scale.z);
    const Vector3 sector_pos = Vector3(p_node->key.sector.cell.x * sector_size * map_scale.x, 0, p_node->key.sector.cell.z * sector_size * map_scale.z);
    const Vector3 cell_pos = Vector3(p_node->key.cell.cell.x * bx.x, p_node->min_y * map_scale.y, p_node->key.cell.cell.z * bz.z);
    const Vector3 origin = cell_pos + sector_pos + world_offset;
    return Transform3D(Basis(bx, by, bz), origin);
}

LODQuadTree::NodeSelectionResult LODQuadTree::_lod_select(const Vector3 &p_viewer_position, const Ref<MapStorage> &p_storage, bool p_parent_inside_frustum, const NodeKey &p_key, uint16_t p_size, int p_lod_level, int p_stop_at_lod_level) {
    hmap_t min_y = 0;
    hmap_t max_y = 0;
    bool has_data = false;
    p_storage->get_minmax(p_key, p_lod_level, min_y, max_y, has_data);
    AABB box = _get_node_AABB(p_key, min_y, max_y, p_size);
    real_t distance_limit = lod_visibility_range[p_lod_level];

    if (!aabb_intersects_sphere(box, p_viewer_position, distance_limit)) {
        return OutOfRange;
    }

    IntersectType frustum_it = p_parent_inside_frustum ? Inside : _aabb_intersects_frustum(box);

    if (frustum_it == Outside) {
        return OutOfFrustum;
    }

    NodeSelectionResult res_subnode_tl = Undefined;
    NodeSelectionResult res_subnode_tr = Undefined;
    NodeSelectionResult res_subnode_bl = Undefined;
    NodeSelectionResult res_subnode_br = Undefined;

    if (p_lod_level > p_stop_at_lod_level) {
        int next_lod = p_lod_level - 1;
        real_t next_distance_limit = lod_visibility_range[next_lod];
        uint16_t x = 2 * p_key.cell.cell.x;
        uint16_t z = 2 * p_key.cell.cell.z;
        uint16_t half_size = p_size / 2;

        if (aabb_intersects_sphere(box, p_viewer_position, next_distance_limit)) {
            bool completely_in_frustum = frustum_it == Inside;
            res_subnode_tl = _lod_select(p_viewer_position, p_storage, completely_in_frustum, NodeKey(p_key.sector, CellKey(x, z)), half_size, next_lod, p_stop_at_lod_level);
            ERR_FAIL_COND_V(res_subnode_tl == MaxReached, MaxReached);
            res_subnode_tr = _lod_select(p_viewer_position, p_storage, completely_in_frustum, NodeKey(p_key.sector, CellKey(x + 1ui16, z)), half_size, next_lod, p_stop_at_lod_level);
            ERR_FAIL_COND_V(res_subnode_tr == MaxReached, MaxReached);
            res_subnode_bl = _lod_select(p_viewer_position, p_storage, completely_in_frustum, NodeKey(p_key.sector, CellKey(x, z + 1ui16)), half_size, next_lod, p_stop_at_lod_level);
            ERR_FAIL_COND_V(res_subnode_bl == MaxReached, MaxReached);
            res_subnode_br = _lod_select(p_viewer_position, p_storage, completely_in_frustum, NodeKey(p_key.sector, CellKey(x + 1ui16, z + 1ui16)), half_size, next_lod, p_stop_at_lod_level);
            ERR_FAIL_COND_V(res_subnode_br == MaxReached, MaxReached);
        } else {
            uint16_t sector_x = p_key.sector.cell.x * sector_size;
            uint16_t sector_z = p_key.sector.cell.z * sector_size;

            if (sector_x + x * half_size >= world_size.x || sector_z + z * half_size >= world_size.y) {
                res_subnode_tl = OutOfMap;
                res_subnode_tr = OutOfMap;
                res_subnode_bl = OutOfMap;
                res_subnode_br = OutOfMap;
            } else {
                if (sector_x + (x + 1ui16) * half_size >= world_size.x) {
                    res_subnode_tr = OutOfMap;
                    res_subnode_br = OutOfMap;
                }

                if (sector_z + (z + 1ui16) * half_size >= world_size.y) {
                    res_subnode_bl = OutOfMap;
                    res_subnode_br = OutOfMap;
                }
            }
        }
    }

    const bool subnode_tl_sel = res_subnode_tl == Selected;
    const bool subnode_tr_sel = res_subnode_tr == Selected;
    const bool subnode_bl_sel = res_subnode_bl == Selected;
    const bool subnode_br_sel = res_subnode_br == Selected;
    const bool remove_subnode_tl = (res_subnode_tl & RESULT_DISCARD) || subnode_tl_sel;
    const bool remove_subnode_tr = (res_subnode_tr & RESULT_DISCARD) || subnode_tr_sel;
    const bool remove_subnode_bl = (res_subnode_bl & RESULT_DISCARD) || subnode_bl_sel;
    const bool remove_subnode_br = (res_subnode_br & RESULT_DISCARD) || subnode_br_sel;

    if (!(remove_subnode_tl && remove_subnode_tr && remove_subnode_bl && remove_subnode_br)) {
        if (selection_count >= MAX_NODE_SELECTION_COUNT) {
            return MaxReached;
        }

        if (has_data) {
            selected_buffer[selection_count] = QTNode(p_key, p_size, min_y, max_y, p_lod_level, !remove_subnode_tl, !remove_subnode_tr, !remove_subnode_bl, !remove_subnode_br);
            selection_count++;
        }

        return Selected;
    }

    if (subnode_tl_sel || subnode_tr_sel || subnode_bl_sel || subnode_br_sel) {
        return Selected; // At least one child has been selected.
    } else {
        return OutOfFrustum;
    }
}

_FORCE_INLINE_ AABB LODQuadTree::_get_node_AABB(const NodeKey &p_key, hmap_t min_y, hmap_t max_y, uint16_t p_size) const {
    const Vector3 sector_position = Vector3(p_key.sector.cell.x * sector_size * chunk_size, 0, p_key.sector.cell.z * sector_size * chunk_size) * map_scale;
    const Vector3 node_size = Vector3(p_size * chunk_size, max_y - min_y, p_size * chunk_size) * map_scale;
    const Vector3 node_position = Vector3(p_key.cell.cell.x * node_size.x, min_y * map_scale.y, p_key.cell.cell.z * node_size.z) + sector_position + world_offset;
    return AABB(node_position, node_size);
}

LODQuadTree::IntersectType LODQuadTree::_aabb_intersects_frustum(const AABB &p_aabb) const {
    int in = 0;

    for (int iplane = 0; iplane < frustum.size(); ++iplane) {
        int out = 0;

        for (int icorner = 0; icorner < 8; ++icorner) {
            Plane plane = frustum[iplane];

            if (plane.is_point_over(p_aabb.get_endpoint(icorner))) {
                out++;
            }
        }

        if (out == 8) {
            return Outside;
        } else if (out == 0) {
            in++;
        }
    }

    return in == frustum.size() ? Inside : Intersects;
}

LODQuadTree::LODQuadTree() {
}

LODQuadTree::~LODQuadTree() {
}
