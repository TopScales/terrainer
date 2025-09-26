/**
 * quad_tree.h
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
#include "utils.h"

class TLODQuadTree : public RefCounted{
    GDCLASS(TLODQuadTree, RefCounted);

    friend class TTerrain;

private:
    static const int MAX_LOD_LEVELS = 15;
    static const int MAX_NODE_SELECTION_COUNT = 4096;
    static const int SEAM_TOP_LEFT = 1;
    static const real_t MORPH_START_RATIO;

    struct QTNode {
        enum LODSelectResult {
            IT_Undefined,
            IT_OutOfFrustum,
            IT_OutOfRange,
            IT_Selected,
        };

        unsigned short x;
        unsigned short z;
        unsigned short size;
        unsigned short level; // NOTE: Highest bit is used to mark leaf nodes.
        unsigned short min_y;
        unsigned short max_y;
        QTNode *sub_tl;
        QTNode *sub_tr;
        QTNode *sub_bl;
        QTNode *sub_br;

        _FORCE_INLINE_ unsigned short get_level() const { return (unsigned short)(level & 0x7FFF); }
        _FORCE_INLINE_ bool is_leaf() const { return (level & 0x8000) != 0; }

        _FORCE_INLINE_ void get_world_minmax_x(int p_raster_size_x, const AABB &p_map_dims, real_t &r_min_x, real_t &r_max_x) const {
            r_min_x = p_map_dims.position.x + x * p_map_dims.size.x / (real_t)(p_raster_size_x - 1);
            r_max_x = p_map_dims.position.x + (x + size) * p_map_dims.size.x / (real_t)(p_raster_size_x - 1);
        }

        _FORCE_INLINE_ void get_world_minmax_y(const AABB &p_map_dims, real_t &r_min_y, real_t &r_max_y) const {
            r_min_y = p_map_dims.position.y + min_y * p_map_dims.size.y / 65535.0;
            r_max_y = p_map_dims.position.y + max_y * p_map_dims.size.y / 65535.0;
        }

        _FORCE_INLINE_ void get_world_minmax_z(int p_raster_size_z, const AABB &p_map_dims, real_t &r_min_z, real_t &r_max_z) const {
            r_min_z = p_map_dims.position.z + z * p_map_dims.size.z / (real_t)(p_raster_size_z - 1);
            r_max_z = p_map_dims.position.z + (z + size) * p_map_dims.size.z / (real_t)(p_raster_size_z - 1);
        }

        // void get_bsphere(const TLODQuadTree &p_quadTree, Vector3 &r_center, real_t &r_radius_squared) const;
        // int fill_subnodes(QTNode *p_nodes[4]) const;
//       LODSelectResult   LODSelect( LODSelectInfo & lodSelectInfo, bool parentCompletelyInFrustum ) const;
//       void              GetAreaMinMaxHeight( int fromX, int fromY, int toX, int toY, float & minZ, float & maxZ, const CDLODQuadTree & quadTree ) const;
//       bool              IntersectRay( const D3DXVECTOR3 & rayOrigin, const D3DXVECTOR3 & rayDirection, float maxDistance, D3DXVECTOR3 & hitPoint, const CDLODQuadTree & quadTree ) const;
    };

    TTerrainData &data;
    real_t view_range = 4000.0;
    Vector2 leaf_node_world_size;
    real_t lod_level_node_diag_sizes[MAX_LOD_LEVELS];
    int root_node_size = 0;
    int root_node_count_x;
    int root_node_count_z;
    Vector<QTNode> nodes;

    void _create_node(QTNode &p_node, int p_x, int p_z, int p_size, int p_level);

public:
    struct SelectedNode
    {
        unsigned int x = 0;
        unsigned int z = 0;
        unsigned short size = 0;
        unsigned short min_y = 0;
        unsigned short max_y = 0;
        Vector3 s;

        unsigned int seams_mask = 0;

        real_t min_dist_to_camera = 1.0;
        int lod_level;

        SelectedNode() {}
        // SelectedNode( const QTNode * node, int LODLevel, bool tl, bool tr, bool bl, bool br );

        // void           GetAABB( AABB & aabb, int rasterSizeX, int rasterSizeY, const MapDimensions & mapDims ) const;
    };

    struct LODSelection {
        SelectedNode selected_buffer[MAX_NODE_SELECTION_COUNT];
        real_t visibility_ranges[MAX_LOD_LEVELS];
        real_t morph_start[MAX_LOD_LEVELS];
        real_t morph_end[MAX_LOD_LEVELS];
    };
//    {
//    private:
//       friend class CDLODQuadTree;
//       friend struct CDLODQuadTree::QTNode;

//       // Input
//       SelectedNode *       m_selectionBuffer;
//       int                  m_maxSelectionCount;
//       D3DXVECTOR3          m_observerPos;
//       float                m_visibilityDistance;
//       D3DXPLANE            m_frustumPlanes[6];
//       float                m_LODDistanceRatio;
//       float                m_morphStartRatio;                  // [0, 1] - when to start morphing to the next (lower-detailed) LOD level; default is 0.667 - first 0.667 part will not be morphed, and the morph will go from 0.667 to 1.0
//       bool                 m_sortByDistance;

//       // Output
//       const CDLODQuadTree * m_quadTree;
//       float                m_visibilityRanges[c_maxLODLevels];
//       float                m_morphEnd[c_maxLODLevels];
//       float                m_morphStart[c_maxLODLevels];
//       int                  m_selectionCount;
//       bool                 m_visDistTooSmall;
//       int                  m_minSelectedLODLevel;
//       int                  m_maxSelectedLODLevel;
    void setup();

    void lod_select(LODSelection *p_selection) const;

    TLODQuadTree(TTerrainData &p_data);
    ~TLODQuadTree();
};

#endif // TERRAINER_QUAD_TREE_H