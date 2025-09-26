#include "lod_quad_tree.h"

const real_t TLODQuadTree::MORPH_START_RATIO = 0.66;

void TLODQuadTree::setup() {

//    clean();

	// Determine the number of nodes to be used, and the size of the root node.
	real_t leaf_size_x = data.quad_tree_leaf_size * data.map_dimensions.size.x / (real_t)data.raster_size.x;
	real_t leaf_size_y = data.quad_tree_leaf_size * data.map_dimensions.size.y / (real_t)data.raster_size.y;
	leaf_node_world_size = Vector2(leaf_size_x, leaf_size_y);
    lod_level_node_diag_sizes[0] = Math::sqrt(leaf_size_x * leaf_size_x + leaf_size_y * leaf_size_y);
    int node_count = 0;
    root_node_size = data.quad_tree_leaf_size;

    for (int i = 0; i < data.lod_level_count; ++i) {
        if (i != 0) {
            root_node_size *= 2;
            lod_level_node_diag_sizes[i] = 2 * lod_level_node_diag_sizes[i - 1];
        }

        int node_count_x = (data.raster_size.x - 1) / root_node_size + 1;
        int node_count_z = (data.raster_size.y - 1) / root_node_size + 1;
        node_count += node_count_x * node_count_z;
    }

    nodes.resize(node_count);
    root_node_count_x = (data.raster_size.x - 1) / root_node_size + 1;
    root_node_count_z = (data.raster_size.y - 1) / root_node_size + 1;

    for (int i = 0; i < node_count; ++i) {
        // _create_node(nodes.get(i));
    }
}

void TLODQuadTree::lod_select(LODSelection *p_selection) const {
    real_t total = 0.0;
    real_t current_detail_balance = 1.0;

    for (int i = 0; i < data.lod_level_count; ++i) {
        total += current_detail_balance;
        current_detail_balance *= data.lod_distance_ratio;
    }

    real_t sect = view_range / total;
    real_t prev_pos = 0.0;
    current_detail_balance = 1.0;

    for (int i = 0; i < data.lod_level_count; ++i) {
        p_selection->visibility_ranges[data.lod_level_count - i - 1] = prev_pos + sect * current_detail_balance;
        prev_pos = p_selection->visibility_ranges[data.lod_level_count - i - 1];
        current_detail_balance *= data.lod_distance_ratio;
    }

    prev_pos = 0.0;

    for (int i = 0; i < data.lod_level_count; ++i) {
        p_selection->morph_end[i] = p_selection->visibility_ranges[data.lod_level_count - i - 1];
        p_selection->morph_start[i] = prev_pos + (p_selection->morph_end[i] - prev_pos) * MORPH_START_RATIO;
        prev_pos = p_selection->morph_start[i];
    }

//    Node::LODSelectInfo lodSelInfo;
//    lodSelInfo.RasterSizeX     = m_rasterSizeX;
//    lodSelInfo.RasterSizeY     = m_rasterSizeY;
//    lodSelInfo.MapDims         = m_desc.MapDims;
//    lodSelInfo.SelectionCount  = 0;
//    lodSelInfo.SelectionObj    = selectionObj;
//    lodSelInfo.StopAtLevel     = layerCount-1;

//    for( int y = 0; y < m_topNodeCountY; y++ )
//       for( int x = 0; x < m_topNodeCountX; x++ )
//       {
//          m_topLevelNodes[y][x]->LODSelect( lodSelInfo, false );
//       }

//    selectionObj->m_maxSelectedLODLevel = 0;
//    selectionObj->m_minSelectedLODLevel = c_maxLODLevels;

//    for( int i = 0; i < lodSelInfo.SelectionCount; i++ )
//    {
//       AABB naabb;
//       selectionObj->m_selectionBuffer[i].GetAABB(naabb, m_rasterSizeX, m_rasterSizeY, m_desc.MapDims);

//       if( selectionObj->m_sortByDistance )
//          selectionObj->m_selectionBuffer[i].MinDistToCamera = sqrtf( naabb.MinDistanceFromPointSq( cameraPos ) );
//       else
//          selectionObj->m_selectionBuffer[i].MinDistToCamera = 0;

//       selectionObj->m_minSelectedLODLevel = ::min( selectionObj->m_minSelectedLODLevel, selectionObj->m_selectionBuffer[i].LODLevel );
//       selectionObj->m_maxSelectedLODLevel = ::max( selectionObj->m_maxSelectedLODLevel, selectionObj->m_selectionBuffer[i].LODLevel );
//    }

//    selectionObj->m_selectionCount      = lodSelInfo.SelectionCount;

//    if( selectionObj->m_sortByDistance )
//       qsort( selectionObj->m_selectionBuffer, selectionObj->m_selectionCount, sizeof(*selectionObj->m_selectionBuffer), compare_closerFirst );
}

void TLODQuadTree::_create_node(QTNode &p_node, int p_x, int p_z, int p_size, int p_level) {
    DEV_ASSERT(p_x >= 0 && p_x < 65535);
    DEV_ASSERT(p_z >= 0 && p_z < 65535);
    DEV_ASSERT(p_level >= 0 && p_level <= MAX_LOD_LEVELS);
    DEV_ASSERT(p_size >= 0 && p_size < 32768);
    p_node.x = (unsigned short)p_x;
    p_node.z = (unsigned short)p_z;
    p_node.level = (unsigned short)p_level;
    p_node.size = (unsigned short)p_size;

    p_node.sub_tl = nullptr;
    p_node.sub_tr = nullptr;
    p_node.sub_bl = nullptr;
    p_node.sub_br = nullptr;

    if (p_size == data.quad_tree_leaf_size) {
        DEV_ASSERT(p_level == data.lod_level_count - 1);
        // Mark leaf node.
        p_node.level |= 0x8000;

        // Find min/max heights at this patch of terrain.
        int limit_x = MIN(data.raster_size.x, p_node.x + p_node.size + 1);
        int limit_z = MIN(data.raster_size.y, p_node.z + p_node.size + 1);
    }

//    // Are we done yet?
//    if( size == (createDesc.LeafRenderNodeSize) )
//    {
//       assert( level == (createDesc.LODLevelCount-1) );

//       // Mark leaf node!
//       Level |= 0x8000;

//       // Find min/max heights at this patch of terrain
//       int limitX = ::min( rasterSizeX, x + size + 1 );
//       int limitY = ::min( rasterSizeY, y + size + 1 );
//       createDesc.pHeightmap->GetAreaMinMaxZ( x, y, limitX - x, limitY - y, this->MinZ, this->MaxZ );


//       // Find heights for 4 corner points (used for approx ray casting)
//       // (reuse otherwise empty pointers used for sub nodes)
//       {
//          float * pTLZ = (float *)&this->SubTL;
//          float * pTRZ = (float *)&this->SubTR;
//          float * pBLZ = (float *)&this->SubBL;
//          float * pBRZ = (float *)&this->SubBR;

//          int limitX = ::min( rasterSizeX - 1, x + size );
//          int limitY = ::min( rasterSizeY - 1, y + size );
//          *pTLZ = createDesc.MapDims.MinZ + createDesc.pHeightmap->GetHeightAt( x, y ) * createDesc.MapDims.SizeZ / 65535.0f;
//          *pTRZ = createDesc.MapDims.MinZ + createDesc.pHeightmap->GetHeightAt( limitX, y ) * createDesc.MapDims.SizeZ / 65535.0f;
//          *pBLZ = createDesc.MapDims.MinZ + createDesc.pHeightmap->GetHeightAt( x, limitY ) * createDesc.MapDims.SizeZ / 65535.0f;
//          *pBRZ = createDesc.MapDims.MinZ + createDesc.pHeightmap->GetHeightAt( limitX, limitY ) * createDesc.MapDims.SizeZ / 65535.0f;
//       }
//    }
//    else
//    {
//       int subSize = size / 2;

//       this->SubTL = &allNodesBuffer[allNodesBufferLastIndex++];
//       this->SubTL->Create( x, y, subSize, level+1, createDesc, allNodesBuffer, allNodesBufferLastIndex );
//       this->MinZ = this->SubTL->MinZ;
//       this->MaxZ = this->SubTL->MaxZ;

//       if( (x + subSize) < rasterSizeX )
//       {
//          this->SubTR = &allNodesBuffer[allNodesBufferLastIndex++];
//          this->SubTR->Create( x + subSize, y, subSize, level+1, createDesc, allNodesBuffer, allNodesBufferLastIndex );
//          this->MinZ = ::min( this->MinZ, this->SubTR->MinZ );
//          this->MaxZ = ::max( this->MaxZ, this->SubTR->MaxZ );
//       }

//       if( (y + subSize) < rasterSizeY )
//       {
//          this->SubBL = &allNodesBuffer[allNodesBufferLastIndex++];
//          this->SubBL->Create( x, y + subSize, subSize, level+1, createDesc, allNodesBuffer, allNodesBufferLastIndex );
//          this->MinZ = ::min( this->MinZ, this->SubBL->MinZ );
//          this->MaxZ = ::max( this->MaxZ, this->SubBL->MaxZ );
//       }

//       if( ((x + subSize) < rasterSizeX) && ((y + subSize) < rasterSizeY) )
//       {
//          this->SubBR = &allNodesBuffer[allNodesBufferLastIndex++];
//          this->SubBR->Create( x + subSize, y + subSize, subSize, level+1, createDesc, allNodesBuffer, allNodesBufferLastIndex );
//          this->MinZ = ::min( this->MinZ, this->SubBR->MinZ );
//          this->MaxZ = ::max( this->MaxZ, this->SubBR->MaxZ );
//       }
//    }
}

TLODQuadTree::TLODQuadTree(TTerrainData &p_data) : data(p_data) {

}

TLODQuadTree::~TLODQuadTree() {

}
