/**
 * sector.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_SECTOR_H
#define TERRAINER_SECTOR_H

#include "region.h"

namespace Terrainer {

class Sector {

using hmap_t = Region::hmap_t;
using CellKey = Region::CellKey;
using RegionSpecs = Region::Specs;
using Size = Region::Size;

private:
    using MinMax = Region::MinMax;

    const RegionSpecs &specs;
    CellKey region_offset = {0, 0};
    MinMax *minmax_buffer = nullptr;
    hmap_t *hmap_buffer = nullptr;
    Vector<Region *> regions;
    float *node_buffer = nullptr;

public:
    struct NodeKey {
        CellKey sector;
        CellKey cell;

        constexpr NodeKey() {}
        constexpr NodeKey(CellKey p_sector, CellKey p_cell) : sector(p_sector), cell(p_cell) {}
        constexpr bool operator==(const NodeKey &p_k) const { return sector == p_k.sector && cell == p_k.cell; }

        _FORCE_INLINE_ Vector3 sector_position(real_t p_scale_x, real_t p_scale_z) const {
            return sector.position(p_scale_x, p_scale_z);
        }
        _FORCE_INLINE_ Vector3 position(int p_sector_size, int p_lod, int p_num_lods, real_t p_scale_x, real_t p_scale_z) const {
            int lod_shift = p_num_lods - p_lod - 1;
            int cell_size = p_sector_size >> lod_shift;
            return Vector3((sector.x * p_sector_size + cell.x * cell_size) * p_scale_x, 0.0, (sector.z * p_sector_size + cell.z * cell_size) * p_scale_x);
        }
        _FORCE_INLINE_ NodeKey next_lod() const {
            CellKey next_cell = CellKey(cell.x >> 1, cell.z >> 1);
            return NodeKey(sector, next_cell);
        }

        uint32_t hash() const {
            uint32_t h = sector.hash();
            h = hash_murmur3_one_32(cell.hash(), h);
            return hash_fmix32(h);
        }
    };
    static_assert(sizeof(NodeKey) == 8);

    struct TextureLayerData {
        PackedFloat32Array heights;
        PackedByteArray normals;
        uint64_t frame = 0;
        NodeKey key;
        int lod = 0;
        bool free = true;
    };

    void get_minmax(const CellKey &p_key, int p_lod, hmap_t &r_min, hmap_t &r_max) const;
    // PackedFloat32Array get_hmap(const CellKey &p_key, int p_lod) const;
    void get_layer_data(const CellKey &p_key, int p_lod, TextureLayerData &r_layer_data) const;

    Sector(const CellKey &p_sector, HashMap<CellKey, Region *> &p_regions, const RegionSpecs &p_specs);
    ~Sector();
};

} // namespace Terrainer

#endif // TERRAINER_SECTOR_H

