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

private:
    using MinMax = Region::MinMax;

    const RegionSpecs &specs;
    CellKey region_offset = {0, 0};
    MinMax *minmax_buffer = nullptr;
    Vector<Region *> regions;

public:
    void get_minmax(const CellKey &p_key, int p_lod, hmap_t &r_min, hmap_t &r_max) const;

    Sector(const CellKey &p_sector, HashMap<CellKey, Region *> &p_regions, const RegionSpecs &p_specs);
    ~Sector();
};

} // namespace Terrainer

#endif // TERRAINER_SECTOR_H

