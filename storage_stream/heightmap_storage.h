/**
 * heightmap_storage.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_HEIGHTMAP_STORAGE_H
#define TERRAINER_HEIGHTMAP_STORAGE_H

#include "stream_storage.h"
#include "data.h"

class THeightmapStorage : public TStreamStorage {
    GDCLASS(THeightmapStorage, TStreamStorage);

    // friend class TLODQuadTree;

private:
    struct MinMaxData {
        uint16_t min = 0;
        uint16_t max = 0;

        MinMaxData(uint16_t p_min, uint16_t p_max) : min(p_min), max(p_max) {}
    };

    struct HeightmapBlock : Block {
        Vector<Vector<MinMaxData>> minmax_maps;
    };

    HeightmapBlock *_create_block(Ref<FileAccess> p_file) const override;
    TTerrainData &data;

public:
    void get_minmax(int p_lod_level, int p_x, int p_y, uint16_t &r_min_y, uint16_t &r_max_y) const;

    THeightmapStorage(TTerrainData &p_data);
    ~THeightmapStorage();
};

#endif // TERRAINER_HEIGHTMAP_STORAGE_H