/**
 * lod_quad_tree.cpp
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#include "lod_quad_tree.h"
#include "utils.h"
// #include "heightmap_storage.h"

// const real_t TLODQuadTree::MORPH_START_RATIO = 0.66;

void TLODQuadTree::set_lod_levels(real_t p_far_view, int p_lod_detailed_chunks_radius, real_t p_morph_start_ratio) {
    int levels = 1;
    real_t level_radius = p_lod_detailed_chunks_radius * info->chunk_size * MAX(info->map_scale.x, info->map_scale.z);
    real_t current_radius = level_radius;

    while (current_radius < p_far_view) {
        level_radius *= info->lod_distance_ratio;
        current_radius += level_radius;
        levels++;
    }

    if (current_radius <= 2.0 * p_far_view) {
        levels -= (int)Math::round((current_radius - p_far_view) / p_far_view); // Try to better fit number of levels to far view distance.
    }

    lod_visibility_range.resize(levels);
    level_radius = 1.0;
    current_radius = 1.0;

    for (int i = 0; i < levels - 1; ++i) {
        lod_visibility_range.set(i, current_radius);
        level_radius *= info->lod_distance_ratio;
        current_radius += level_radius;
    }

    lod_visibility_range.set(levels - 1, p_far_view);
    info->lod_levels = levels;
    real_t m = p_far_view / current_radius;

    for (int i = 0; i < levels - 1; ++i) {
        lod_visibility_range.set(i, lod_visibility_range[i] * m);
    }

    real_t prev_pos = 0.0;
    selection_morph_end.resize(levels);
    selection_morph_start.resize(levels);
    info->root_node_size = info->chunk_size;

    for (int i = 0; i < levels; ++i) {
        real_t end = lod_visibility_range[i];
        real_t start = prev_pos + (end - prev_pos) * p_morph_start_ratio;
        selection_morph_end.set(i, end);
        selection_morph_start.set(i, start);
        prev_pos = start;
        info->root_node_size *= 2;
    }

    info->root_nodes_count_x = MAX(1, info->world_blocks.x * info->block_size * info->chunk_size / info->root_node_size);
    info->root_nodes_count_z = MAX(1, info->world_blocks.y * info->block_size * info->chunk_size / info->root_node_size);
}

void TLODQuadTree::select_nodes(const Vector3 &p_viewer_position, int p_stop_at_lod_level) {
    selection_count = 0;
    int16_t ix0 = -info->root_nodes_count_x / 2;
    int16_t ix1 = -ix0 + info->root_nodes_count_x % 2;
    int16_t iz0 = -info->root_nodes_count_z / 2;
    int16_t iz1 = -iz0 + info->root_nodes_count_z % 2;
    int top_lod = MAX(0, info->lod_levels - 1);

    for (int16_t iz = iz0; iz < iz1; ++iz) {
        for (int16_t ix = ix0; ix < ix1; ++ix) {
            _lod_select(p_viewer_position, false, ix, iz, info->root_node_size, top_lod, 0);
        }
    }

    info->min_selected_lod = MAX_LOD_LEVELS;
    info->max_selected_lod = 0;

    for (int i = 0; i < selection_count; ++i) {
        int selected_node_lod_level = selected_buffer[i].get_lod_level();
        info->min_selected_lod = MIN(info->min_selected_lod, selected_node_lod_level);
        info->max_selected_lod = MAX(info->max_selected_lod, selected_node_lod_level);
    }
}

int TLODQuadTree::get_selection_count() const {
    return selection_count;
}

AABB TLODQuadTree::get_selected_node_aabb(int p_index) const {
    ERR_FAIL_INDEX_V_EDMSG(p_index, selection_count, AABB(), "Selected node index out of bounds.");
    const QTNode &node = selected_buffer[p_index];
    Vector3 size = Vector3(node.size, node.max_y - node.min_y, node.size) * info->map_scale;
    Vector3 position = Vector3(node.x * size.x, node.min_y * info->map_scale.y, node.z * size.z);
    return AABB(position, size);
}

int TLODQuadTree::get_selected_node_lod(int p_index) const {
    ERR_FAIL_INDEX_V_EDMSG(p_index, selection_count, 0, "Selected node index out of bounds.");
    const QTNode &node = selected_buffer[p_index];
    return node.get_lod_level();
}

// void TLODQuadTree::_create_node(QTNode &p_node, int p_x, int p_z, int p_size, int p_level) {
//     DEV_ASSERT(p_x >= 0 && p_x < 65535);
//     DEV_ASSERT(p_z >= 0 && p_z < 65535);
//     DEV_ASSERT(p_level >= 0 && p_level <= MAX_LOD_LEVELS);
//     DEV_ASSERT(p_size >= 0 && p_size < 32768);
//     p_node.x = (unsigned short)p_x;
//     p_node.z = (unsigned short)p_z;
//     p_node.level = (unsigned short)p_level;
//     p_node.size = (unsigned short)p_size;

//     p_node.sub_tl = nullptr;
//     p_node.sub_tr = nullptr;
//     p_node.sub_bl = nullptr;
//     p_node.sub_br = nullptr;

//     if (p_size == data.qt_leaf_node_size) {
//         DEV_ASSERT(p_level == data.lod_level_count - 1);
//         // Mark leaf node.
//         p_node.level |= 0x8000;

//         // Find min/max heights at this patch of terrain.
//         int limit_x = MIN(data.raster_size.x, p_node.x + p_node.size + 1);
//         int limit_z = MIN(data.raster_size.y, p_node.z + p_node.size + 1);
//     }
// }

TLODQuadTree::NodeSelectionResult TLODQuadTree::_lod_select(const Vector3 &p_viewer_position, bool p_parent_inside_frustum, int16_t p_x, int16_t p_z, uint16_t p_size, int p_lod_level, int p_stop_at_lod_level) {
    uint16_t min_y = 0;
    uint16_t max_y = 1; // TODO: Get minmax from heightmap data.
    Vector3 node_size = Vector3(p_size, max_y - min_y, p_size) * info->map_scale;
    Vector3 node_position = Vector3(p_x * node_size.x, min_y * info->map_scale.y, p_z * node_size.z);
    AABB box = AABB(node_position, node_size);
    real_t distance_limit = lod_visibility_range[p_lod_level];

    if (!t_aabb_intersects_sphere(box, p_viewer_position, distance_limit)) {
        return RESULT_OUT_OF_RANGE;
    }

    IntersectType frustum_it = _aabb_intersects_frustum(box);

    if (frustum_it == OUTSIDE) {
        return RESULT_OUT_OF_FRUSTUM;
    }

    NodeSelectionResult res_subnode_tl = RESULT_UNDEFINED;
    NodeSelectionResult res_subnode_tr = RESULT_UNDEFINED;
    NodeSelectionResult res_subnode_bl = RESULT_UNDEFINED;
    NodeSelectionResult res_subnode_br = RESULT_UNDEFINED;

    if (p_lod_level > p_stop_at_lod_level) {
        real_t next_distance_limit = lod_visibility_range[p_lod_level - 1];

        if (t_aabb_intersects_sphere(box, p_viewer_position, next_distance_limit)) {
            bool completely_in_frustum = frustum_it == INSIDE;
            // TODO: Check if subnodes exist.
            int half_size = p_size / 2;
            int16_t x = 2 * p_x;
            int16_t z = 2 * p_z;
            res_subnode_tl = _lod_select(p_viewer_position, completely_in_frustum, x, z, half_size, p_lod_level - 1, p_stop_at_lod_level);
            ERR_FAIL_COND_V(res_subnode_tl == RESULT_MAX_REACHED, RESULT_MAX_REACHED);
            res_subnode_tr = _lod_select(p_viewer_position, completely_in_frustum, x + 1i16, z, half_size, p_lod_level - 1, p_stop_at_lod_level);
            ERR_FAIL_COND_V(res_subnode_tr == RESULT_MAX_REACHED, RESULT_MAX_REACHED);
            res_subnode_bl = _lod_select(p_viewer_position, completely_in_frustum, x, z + 1i16, half_size, p_lod_level - 1, p_stop_at_lod_level);
            ERR_FAIL_COND_V(res_subnode_bl == RESULT_MAX_REACHED, RESULT_MAX_REACHED);
            res_subnode_br = _lod_select(p_viewer_position, completely_in_frustum, x + 1i16, z + 1i16, half_size, p_lod_level - 1, p_stop_at_lod_level);
            ERR_FAIL_COND_V(res_subnode_br == RESULT_MAX_REACHED, RESULT_MAX_REACHED);
        }
    }

    bool subnode_tl_sel = res_subnode_tl == RESULT_SELECTED;
    bool subnode_tr_sel = res_subnode_tr == RESULT_SELECTED;
    bool subnode_bl_sel = res_subnode_bl == RESULT_SELECTED;
    bool subnode_br_sel = res_subnode_br == RESULT_SELECTED;
    bool remove_subnode_tl = res_subnode_tl == RESULT_OUT_OF_FRUSTUM || subnode_tl_sel;
    bool remove_subnode_tr = res_subnode_tr == RESULT_OUT_OF_FRUSTUM || subnode_tr_sel;
    bool remove_subnode_bl = res_subnode_bl == RESULT_OUT_OF_FRUSTUM || subnode_bl_sel;
    bool remove_subnode_br = res_subnode_br == RESULT_OUT_OF_FRUSTUM || subnode_br_sel;

    if (!(remove_subnode_tl && remove_subnode_tr && remove_subnode_bl && remove_subnode_br) || info->include_all_nodes_in_range) {
        if (selection_count >= MAX_NODE_SELECTION_COUNT - 1) {
            return RESULT_MAX_REACHED;
        }

        selected_buffer[selection_count] = QTNode(p_x, p_z, p_size, min_y, max_y, p_lod_level, !remove_subnode_tl, !remove_subnode_tr, !remove_subnode_bl, !remove_subnode_br);
        selection_count++;
        return RESULT_SELECTED;
        // TODO: Check if morph range is too large.
    }

    ERR_FAIL_COND_V_EDMSG(!(subnode_tl_sel || subnode_tr_sel || subnode_bl_sel || subnode_br_sel), RESULT_OUT_OF_FRUSTUM, "Node is inside frustum, but none of its childs is.");
    return RESULT_SELECTED; // At least one child should have been selected.
}

TLODQuadTree::IntersectType TLODQuadTree::_aabb_intersects_frustum(const AABB &p_aabb) const {
    int in = 0;
    for (int i = 0; i < 8; ++i) {
        bool inside = true;
        for (int j = 0; j < info->frustum.size(); j++) {
            if (info->frustum[j].is_point_over(p_aabb.get_endpoint(i))) {
                inside = false;
                break;
            }
        }

        if (inside) {
            in++;
        }
    }

    return in == 8 ? INSIDE : (in == 0 ? OUTSIDE : INTERSECTS);
}

TLODQuadTree::TLODQuadTree() {
}

TLODQuadTree::~TLODQuadTree() {

}
