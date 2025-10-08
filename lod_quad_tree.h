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

#include "core/object/ref_counted.h"
#include "terrain_info.h"
#include "scene/3d/camera_3d.h"

#define DEFAULT_MORPH_START_RATIO  (0.70)

class TLODQuadTree : public RefCounted{
    GDCLASS(TLODQuadTree, RefCounted);

private:
    static const int MAX_LOD_LEVELS = 15;
    static const int MAX_NODE_SELECTION_COUNT = 4096;
    static const int TL_BIT = 1 << 4;
    static const int TR_BIT = 1 << 5;
    static const int BL_BIT = 1 << 6;
    static const int BR_BIT = 1 << 7;
    static constexpr real_t LOD0_RADIUS_FACTOR = 1.42;

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

    TTerrainInfo *info;
    QTNode selected_buffer[MAX_NODE_SELECTION_COUNT];
    Vector<real_t> lod_visibility_range;
    int selection_count = 0;
    int min_selected_lod_level = 0;
    int max_selected_lod_level = 0;
    Vector<real_t> selection_morph_end;
    Vector<real_t> selection_morph_start;

//     struct QTNode {
//         unsigned short x;
//         unsigned short z;
//         unsigned short size;
//         unsigned short level; // NOTE: Highest bit is used to mark leaf nodes.
//         unsigned short min_y;
//         unsigned short max_y;
//         QTNode *sub_tl;
//         QTNode *sub_tr;
//         QTNode *sub_bl;
//         QTNode *sub_br;

//         _FORCE_INLINE_ unsigned short get_level() const { return (unsigned short)(level & 0x7FFF); }
//         _FORCE_INLINE_ bool is_leaf() const { return (level & 0x8000) != 0; }

//         _FORCE_INLINE_ void get_world_minmax_x(int p_raster_size_x, const AABB &p_map_dims, real_t &r_min_x, real_t &r_max_x) const {
//             r_min_x = p_map_dims.position.x + x * p_map_dims.size.x / (real_t)(p_raster_size_x - 1);
//             r_max_x = p_map_dims.position.x + (x + size) * p_map_dims.size.x / (real_t)(p_raster_size_x - 1);
//         }

//         _FORCE_INLINE_ void get_world_minmax_y(const AABB &p_map_dims, real_t &r_min_y, real_t &r_max_y) const {
//             r_min_y = p_map_dims.position.y + min_y * p_map_dims.size.y / 65535.0;
//             r_max_y = p_map_dims.position.y + max_y * p_map_dims.size.y / 65535.0;
//         }

//         _FORCE_INLINE_ void get_world_minmax_z(int p_raster_size_z, const AABB &p_map_dims, real_t &r_min_z, real_t &r_max_z) const {
//             r_min_z = p_map_dims.position.z + z * p_map_dims.size.z / (real_t)(p_raster_size_z - 1);
//             r_max_z = p_map_dims.position.z + (z + size) * p_map_dims.size.z / (real_t)(p_raster_size_z - 1);
//         }

//         // void get_bsphere(const TLODQuadTree &p_quadTree, Vector3 &r_center, real_t &r_radius_squared) const;
//         // int fill_subnodes(QTNode *p_nodes[4]) const;
// //       LODSelectResult   LODSelect( LODSelectInfo & lodSelectInfo, bool parentCompletelyInFrustum ) const;
// //       void              GetAreaMinMaxHeight( int fromX, int fromY, int toX, int toY, float & minZ, float & maxZ, const CDLODQuadTree & quadTree ) const;
// //       bool              IntersectRay( const D3DXVECTOR3 & rayOrigin, const D3DXVECTOR3 & rayDirection, float maxDistance, D3DXVECTOR3 & hitPoint, const CDLODQuadTree & quadTree ) const;
//     };

//     struct SelectedQTNode
//     {
//         unsigned int x = 0;
//         unsigned int z = 0;
//         unsigned short size = 0;
//         unsigned short min_y = 0;
//         unsigned short max_y = 0;
//         Vector3 s;

//         unsigned int seams_mask = 0;

//         real_t min_dist_to_camera = 1.0;
//         int lod_level;

//         SelectedQTNode() {}
//         // SelectedNode( const QTNode * node, int LODLevel, bool tl, bool tr, bool bl, bool br );

//         // void           GetAABB( AABB & aabb, int rasterSizeX, int rasterSizeY, const MapDimensions & mapDims ) const;
//     };

//     struct LODSelection {

//         real_t visibility_ranges[MAX_LOD_LEVELS];
//         real_t morph_start[MAX_LOD_LEVELS];
//         real_t morph_end[MAX_LOD_LEVELS];
//     };

//     Vector2 leaf_node_world_size;
//     real_t lod_level_node_diag_sizes[MAX_LOD_LEVELS];
//     int root_node_size = 0;
//     int root_node_count_x = 0;
//     int root_node_count_z = 0;
//     Vector<QTNode> nodes;
//     bool use_frustum_cull = true;

//     void _select_nodes(LODSelection &p_selection);
//     void _create_node(QTNode &p_node, int p_x, int p_z, int p_size, int p_level);
    NodeSelectionResult _lod_select(const Vector3 &p_viewer_position, bool p_parent_inside_frustum, int16_t p_x, int16_t p_z, uint16_t p_size, int p_lod_level, int p_stop_at_lod_level);
    _FORCE_INLINE_ IntersectType _aabb_intersects_frustum(const AABB &p_aabb) const;

public:
    void set_lod_levels(real_t p_far_view, int p_lod_detailed_chunks_radius, real_t p_morph_start_ratio = DEFAULT_MORPH_START_RATIO);
    void select_nodes(const Vector3 &p_viewer_position, int p_stop_at_lod_level = 0);
    int get_selection_count() const;
    AABB get_selected_node_aabb(int p_index) const;
    int get_selected_node_lod(int p_index) const;
    void set_info(TTerrainInfo *p_info) { info = p_info; }

    TLODQuadTree();
    ~TLODQuadTree();
};

#endif // TERRAINER_QUAD_TREE_H