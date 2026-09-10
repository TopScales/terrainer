/**
 * sector.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#include "sector.h"

using namespace Terrainer;

void Sector::get_minmax(const CellKey &p_key, int p_lod, hmap_t &r_min, hmap_t &r_max) const {
    const size_t side = (specs.region_size * specs.sector_regions) >> p_lod;

    if (p_lod < specs.region_lods) {
        const int region_blocks = side / specs.sector_regions;
        const int region_ix = p_key.x / region_blocks;
        const int region_iz = p_key.z / region_blocks;
        const int region_idx = region_ix + region_iz * side;
        const int block_idx = (int)p_key.x - region_ix * region_blocks + ((int)p_key.z - region_iz * region_blocks) * region_blocks;
        const MinMax minmax = regions[region_idx] ? regions[region_idx]->get_minmax(p_lod, block_idx) : specs.default_minmax;
        r_min = minmax.min;
        r_max = minmax.max;
        // TODO: Consider region offset.
    } else {
        const int block_idx = p_key.x + p_key.z * side;
        const MinMax &minmax = *(minmax_buffer + specs.sector_minmax_lod_offsets[p_lod - specs.region_lods] + block_idx);
        r_min = minmax.min;
        r_max = minmax.max;
    }
}

Sector::Sector(const CellKey &p_sector, HashMap<CellKey, Region*> &p_regions, const RegionSpecs &p_specs)
    : specs(p_specs)
{
    real_t nreg = (real_t)specs.region_size / (real_t)specs.sector_size;
    CellKey region0 = {static_cast<uint16_t>((real_t)p_sector.x * nreg), static_cast<uint16_t>((real_t)p_sector.z * nreg)};
    regions.resize(specs.sector_regions);
    int idx = 0;

    for (size_t reg_iz = 0; reg_iz < specs.sector_regions; ++reg_iz) {
        for (size_t reg_ix = 0; reg_ix < specs.sector_regions; ++reg_ix) {
            CellKey region_key = region0 + CellKey(reg_ix, reg_iz);
            Region **region_ptr = p_regions.getptr(region_key);
            regions.write[idx] = region_ptr ? *region_ptr : nullptr;
            idx++;
        }
    }

    if (nreg < 1.0) {
        region_offset = p_sector - CellKey(region0.x / nreg, region0.z / nreg);
    }

    if (specs.sector_regions > 1) {
        minmax_buffer = (MinMax *)memalloc(specs.sector_minmax_buffer_size * sizeof(MinMax));
        idx = 0;

        for (size_t reg_iz = 0; reg_iz < specs.sector_regions; reg_iz += 2) {
            for (size_t reg_ix = 0; reg_ix < specs.sector_regions; reg_ix += 2) {
                const size_t idx00 = reg_ix + reg_iz * specs.sector_regions;
                const size_t idx10 = idx00 + 1;
                const size_t idx01 = idx10 + specs.sector_regions;
                const size_t idx11 = idx01 + 1;
                const MinMax mm00 = regions[idx00] ? regions[idx00]->get_minmax(specs.region_lods - 1, 0) : specs.default_minmax;
                const MinMax mm10 = regions[idx10] ? regions[idx10]->get_minmax(specs.region_lods - 1, 0) : specs.default_minmax;
                const MinMax mm01 = regions[idx01] ? regions[idx01]->get_minmax(specs.region_lods - 1, 0) : specs.default_minmax;
                const MinMax mm11 = regions[idx11] ? regions[idx11]->get_minmax(specs.region_lods - 1, 0) : specs.default_minmax;
                minmax_buffer[idx].min = MIN(mm00.min, MIN(mm10.min, MIN(mm01.min, mm11.min)));
                minmax_buffer[idx].max = MAX(mm00.max, MAX(mm10.max, MAX(mm01.max, mm11.max)));
                idx++;
            }
        }

        int extra_lods = specs.lods - specs.region_lods;
        size_t rsize = specs.sector_regions;
        MinMax *parent = minmax_buffer;
        MinMax *minmax_ptr = minmax_buffer + idx;

        for (int ilod = 1; ilod < extra_lods; ++ilod) {
            MinMax *next_parent = minmax_ptr;

            for (size_t reg_iz = 0; reg_iz < rsize; reg_iz += 2) {
                for (size_t reg_ix = 0; reg_ix < rsize; reg_ix += 2) {
                    const size_t idx00 = reg_ix + reg_iz * rsize;
                    const size_t idx10 = idx00 + 1;
                    const size_t idx01 = idx10 + rsize;
                    const size_t idx11 = idx01 + 1;
                    minmax_ptr->min = MIN(parent[idx00].min, MIN(parent[idx10].min, MIN(parent[idx01].min, parent[idx11].min)));
                    minmax_ptr->max = MAX(parent[idx00].max, MAX(parent[idx10].max, MAX(parent[idx01].max, parent[idx11].max)));
                    minmax_ptr++;
                }
            }

            rsize >>= 1;
            parent = next_parent;
        }
    }
}

Sector::~Sector() {
    if (minmax_buffer) {
        memfree(minmax_buffer);
    }
}

