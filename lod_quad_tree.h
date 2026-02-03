/**
 * lod_quad_tree.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_QUAD_TREE_H
#define TERRAINER_QUAD_TREE_H

// #include "map_storage/minmax_map.h"
// #include "terrain_info.h"

#ifdef TERRAINER_MODULE
#include "scene/3d/camera_3d.h"
#include "scene/resources/image_texture.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/classes/image_texture.hpp>
#endif // TERRAINER_GDEXTENSION

#define DEFAULT_MORPH_START_RATIO  (0.66)

namespace Terrainer {

class LODQuadTree {

    friend class Terrain;

private:
    static const int MAX_LOD_LEVELS = 15;
    static const uint8_t LOD_MASK = 0x0F;
    static const uint8_t TL_BIT = 1 << 4;
    static const uint8_t TR_BIT = 1 << 5;
    static const uint8_t BL_BIT = 1 << 6;
    static const uint8_t BR_BIT = 1 << 7;
    static constexpr real_t LOD0_RADIUS_FACTOR = 1.2;
    static const int MAX_NODE_SELECTION_COUNT = 4096;

    enum NodeSelectionResult {
		Undefined = 0,
		OutOfFrustum = 1,
		OutOfRange = 2,
        OutOfMap = 4,
        Selected = 8,
        MaxReached = 16
	};

    static constexpr int RESULT_DISCARD = OutOfFrustum | OutOfMap;

    enum IntersectType
    {
        Outside,
        Intersects,
        Inside
    };

    struct QTNode {
        uint16_t x = 0;
        uint16_t z = 0;
        uint16_t size = 1;
        uint16_t min_y = 0;
        uint16_t max_y = 0;
        uint8_t flags = 0;

        _FORCE_INLINE_ int get_lod_level() const { return flags & LOD_MASK; }
        _FORCE_INLINE_ bool use_tl() const { return flags & TL_BIT; }
        _FORCE_INLINE_ bool use_tr() const { return flags & TL_BIT; }
        _FORCE_INLINE_ bool use_bl() const { return flags & TL_BIT; }
        _FORCE_INLINE_ bool use_br() const { return flags & TL_BIT; }

        QTNode() {};

        QTNode(int16_t p_x, int16_t p_z, uint16_t p_size, uint16_t p_min_y, uint16_t p_max_y, int p_lod_level, \
            bool p_use_tl, bool p_use_tr, bool p_use_bl, bool p_use_br)
        : x(p_x), z(p_z), size(p_size), min_y(p_min_y), max_y(p_max_y) {
            flags = (p_lod_level & LOD_MASK) | (TL_BIT * p_use_tl) | (TR_BIT * p_use_tr) | (BL_BIT * p_use_bl) | (BR_BIT * p_use_br);
        }
    };

    QTNode selected_buffer[MAX_NODE_SELECTION_COUNT];

    int chunk_size = 0;
    int region_size = 0;
    Vector2i world_size; // In number of chunks.
    Vector3 map_scale;

    uint16_t sector_size = 1; // In number of chunks.
    uint16_t sector_count_x = 1;
    uint16_t sector_count_z = 1;
    real_t lod_distance_ratio = 2.0;

    int lod_levels = 0;
    Vector<real_t> lod_visibility_range;
    int selection_count = 0;
    Vector<int> lods_count;
    Vector3 world_offset;

#ifdef TERRAINER_MODULE
    Vector<Plane> frustum;
#elif TERRAINER_GDEXTENSION
    TypedArray<Plane> frustum;
#endif
//     NodeSelectionResult _lod_select(const Vector3 &p_viewer_position, const TMinmaxMap &p_minmax_map, bool p_parent_inside_frustum, uint16_t p_x, uint16_t p_z, uint16_t p_size, int p_lod_level, int p_stop_at_lod_level);
//     _FORCE_INLINE_ AABB _get_node_AABB(uint16_t p_x, uint16_t p_z, uint16_t min_y, uint16_t max_y, uint16_t p_size) const;
//     _FORCE_INLINE_ IntersectType _aabb_intersects_frustum(const AABB &p_aabb) const;

public:
    void set_map_info(int p_chunk_size, int p_region_size, const Vector2i p_world_regions, const Vector3 &p_map_scale);
    void set_lod_levels(real_t p_far_view, int p_lod_detailed_chunks_radius);
    // void select_nodes(const Vector3 &p_viewer_position, const TMinmaxMap &p_minmax_map, int p_stop_at_lod_level = 0);
//     const QTNode *get_selected_node(int p_index) const;
//     AABB get_selected_node_aabb(int p_index) const;
//     int get_selected_node_lod(int p_index) const;
//     void set_info(TTerrainInfo *p_info) { info = p_info; }
//     void set_world_info(TWorldInfo *p_info) { world_info = p_info; }
    int get_lod_nodes_count(int p_level) const;
//     Ref<ImageTexture> get_morph_texture(real_t p_morph_start_ratio = DEFAULT_MORPH_START_RATIO) const;
//     Transform3D get_node_transform(const QTNode *p_node) const;

    LODQuadTree();
    ~LODQuadTree();
};

} // namespace Terrainer

#endif // TERRAINER_QUAD_TREE_H