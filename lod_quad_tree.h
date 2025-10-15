/**
 * lod_quad_tree.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_QUAD_TREE_H
#define TERRAINER_QUAD_TREE_H

#include "terrain_info.h"

#ifdef TERRAINER_MODULE
#include "scene/3d/camera_3d.h"
#include "scene/resources/image_texture.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/classes/image_texture.hpp>
#endif // TERRAINER_GDEXTENSION

#define DEFAULT_MORPH_START_RATIO  (0.66)

class TLODQuadTree {

public:
    struct QTNode {
        int16_t x = 0;
        int16_t z = 0;
        uint16_t size = 1;
        uint16_t min_y = 0;
        uint16_t max_y = 0;
        uint8_t flags = 0;

        int get_lod_level() const {
            return flags & MAX_LOD_LEVELS;
        }

        bool use_tl() const {
            return flags & TL_BIT;
        }

        bool use_tr() const {
            return flags & TL_BIT;
        }

        bool use_bl() const {
            return flags & TL_BIT;
        }

        bool use_br() const {
            return flags & TL_BIT;
        }

        QTNode() {};

        QTNode(int16_t p_x, int16_t p_z, uint16_t p_size, uint16_t p_min_y, uint16_t p_max_y, int p_lod_level, \
            bool p_use_tl, bool p_use_tr, bool p_use_bl, bool p_use_br)
        : x(p_x), z(p_z), size(p_size), min_y(p_min_y), max_y(p_max_y) {
            flags = (p_lod_level & MAX_LOD_LEVELS) | (TL_BIT * p_use_tl) | (TR_BIT * p_use_tr) | (BL_BIT * p_use_bl) | (BR_BIT * p_use_br);
        }
    };

private:
    static const int MAX_LOD_LEVELS = 15;
    static const int MAX_NODE_SELECTION_COUNT = 4096;
    static const int TL_BIT = 1 << 4;
    static const int TR_BIT = 1 << 5;
    static const int BL_BIT = 1 << 6;
    static const int BR_BIT = 1 << 7;
    static constexpr real_t LOD0_RADIUS_FACTOR = 1.2;

    enum NodeSelectionResult {
		RESULT_UNDEFINED,
		RESULT_OUT_OF_FRUSTUM,
		RESULT_OUT_OF_RANGE,
        RESULT_SELECTED,
        RESULT_MAX_REACHED
	};

    enum IntersectType
    {
        OUTSIDE,
        INTERSECTS,
        INSIDE
    };

    TTerrainInfo *info;
    QTNode selected_buffer[MAX_NODE_SELECTION_COUNT];
    Vector<real_t> lod_visibility_range;
    int selection_count = 0;
    int min_selected_lod_level = 0;
    int max_selected_lod_level = 0;
    Vector<int> lods_count;

    NodeSelectionResult _lod_select(const Vector3 &p_viewer_position, bool p_parent_inside_frustum, int16_t p_x, int16_t p_z, uint16_t p_size, int p_lod_level, int p_stop_at_lod_level);
    _FORCE_INLINE_ IntersectType _aabb_intersects_frustum(const AABB &p_aabb) const;

public:

    void set_lod_levels(real_t p_far_view, int p_lod_detailed_chunks_radius);
    void select_nodes(const Vector3 &p_viewer_position, int p_stop_at_lod_level = 0);
    int get_selection_count() const;
    const QTNode *get_selected_node(int p_index) const;
    AABB get_selected_node_aabb(int p_index) const;
    int get_selected_node_lod(int p_index) const;
    void set_info(TTerrainInfo *p_info) { info = p_info; }
    int get_lod_nodes_count(int p_level) const;
    Ref<ImageTexture> get_morph_texture(real_t p_morph_start_ratio = DEFAULT_MORPH_START_RATIO) const;

    TLODQuadTree();
    ~TLODQuadTree();
};

#endif // TERRAINER_QUAD_TREE_H