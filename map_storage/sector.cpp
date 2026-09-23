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

// #include "../utils/compat_marshalls.h"

using namespace Terrainer;

void Sector::get_minmax(const CellKey &p_key, int p_lod, hmap_t &r_min, hmap_t &r_max) const {
    const size_t side = specs.sector_size >> p_lod;

    if (p_lod < specs.region_lods) {
        const int region_nodes = specs.region_size >> p_lod;
        const int region_ix = p_key.x / region_nodes;
        const int region_iz = p_key.z / region_nodes;
        const int region_idx = region_ix + region_iz * specs.sector_regions;

        if (regions[region_idx]) {
            const int node_ix = (int)p_key.x - region_ix * region_nodes + (region_offset.x >> p_lod);
            const int node_iz = (int)p_key.z - region_iz * region_nodes + (region_offset.z >> p_lod);
            const int node_idx = node_ix + node_iz * region_nodes;
            const MinMax minmax = regions[region_idx]->get_minmax(p_lod, node_idx);
            r_min = minmax.min;
            r_max = minmax.max;
        } else {
            r_min = specs.default_minmax.min;
            r_max = specs.default_minmax.max;
        }
    } else {
        const int block_idx = p_key.x + p_key.z * side;
        const MinMax minmax = *(minmax_buffer + specs.sector_minmax_lod_offsets[p_lod - specs.region_lods] + block_idx);
        r_min = minmax.min;
        r_max = minmax.max;
    }
}

// PackedFloat32Array Sector::get_hmap(const CellKey &p_key, int p_lod) const {
//     const size_t node_size = (specs.chunk_size + 1) * (specs.chunk_size + 1);
//     const hmap_t *hmap_ptr = nullptr;
//     PackedFloat32Array data;
//     data.resize(node_size);
//     float *ptr = data.ptrw();

//     if (p_lod < specs.region_lods) {
//         const int region_nodes = specs.region_size >> p_lod;
//         const int region_ix = p_key.x / region_nodes;
//         const int region_iz = p_key.z / region_nodes;
//         const int region_idx = region_ix + region_iz * specs.sector_regions;

//         if (regions[region_idx]) {
//             const int node_ix = (int)p_key.x - region_ix * region_nodes + (region_offset.x >> p_lod);
//             const int node_iz = (int)p_key.z - region_iz * region_nodes + (region_offset.z >> p_lod);
//             const int node_idx = node_ix + node_iz * region_nodes;
//             hmap_ptr = regions[region_idx]->get_hmap_chunk(p_lod, node_idx);
//         } else {
//             float h = specs.default_height * specs.y_scale;
//             data.fill(h);
//             return data;
//         }
//     } else {
//         const size_t side = specs.sector_size >> p_lod;
//         const int block_idx = p_key.x + p_key.z * side;
//         hmap_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[p_lod - specs.region_lods] + block_idx * node_size;
//     }

//     for (int i = 0; i < node_size; ++i) {
//         float h = hmap_ptr[i] * specs.y_scale;
//         *ptr = h;
//         ptr++;
//     }

//     return data;
// }

void Sector::get_hmap_normal_data(const NodeKey &p_key, int p_lod, TextureLayerData &r_layer_data) const {
    const size_t chunkp1 = specs.chunk_size + 1;
    const size_t node_size = chunkp1 * chunkp1;
    const hmap_t *hmap_ptr = nullptr;
    PackedFloat32Array &heights = r_layer_data.heights;
    heights.resize(node_size);
    float *h_ptr = heights.ptrw();
    PackedByteArray &normals = r_layer_data.normals;
    normals.resize(3 * node_size);
    uint8_t *n_ptr = normals.ptrw();
    const real_t y_scale = specs.scale.y;

    if (p_lod < specs.region_lods) {
        const int region_nodes = specs.region_size >> p_lod;
        const int region_ix = p_key.cell.x / region_nodes;
        const int region_iz = p_key.cell.z / region_nodes;
        const int region_idx = region_ix + region_iz * specs.sector_regions;

        if (regions[region_idx]) {
            const int node_ix = (int)p_key.cell.x - region_ix * region_nodes + (region_offset.x >> p_lod);
            const int node_iz = (int)p_key.cell.z - region_iz * region_nodes + (region_offset.z >> p_lod);
            const int node_idx = node_ix + node_iz * region_nodes;
            hmap_ptr = regions[region_idx]->get_hmap_chunk(p_lod, node_idx);
        } else {
            float h = specs.default_height * y_scale;
            heights.fill(h);

            for (int i = 0; i < node_size; ++i) {
                const int ii = 3 * i;
                n_ptr[ii] = 127;
                n_ptr[ii + 1] = 255;
                n_ptr[ii + 2] = 127;
            }

            return;
        }
    } else {
        const size_t side = specs.sector_size >> p_lod;
        const int block_idx = p_key.cell.x + p_key.cell.z * side;
        hmap_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[p_lod - specs.region_lods] + block_idx * node_size;
    }

    for (int i = 0; i < node_size; ++i) {
        const float h = hmap_ptr[i] * y_scale;
        h_ptr[i] = h;
    }

    const float dx_inv = 1.0f / (2.0f * specs.scale.x);
    const float dz_inv = 1.0f / (2.0f * specs.scale.z);

    for (int iz = 1; iz < specs.chunk_size; ++iz) {
        for (int ix = 1; ix < specs.chunk_size; ++ix) {
            const int idx = ix + iz * chunkp1;
            const int idx_xn = idx - 1;
            const int idx_xp = idx + 1;
            const int idx_zn = idx - chunkp1;
            const int idx_zp = idx + chunkp1;
            const float dh_dx = (hmap_ptr[idx_xp] - hmap_ptr[idx_xn]) * dx_inv;
            const float dh_dz = (hmap_ptr[idx_zp] - hmap_ptr[idx_zn]) * dz_inv;
            const Vector3 n = Vector3(dh_dx, 1.0, dh_dz).normalized();
            const int ii = 3 * idx;
            n_ptr[ii] = uint8_t((0.5 * n.x + 0.5) * 255.0);
            n_ptr[ii + 1] = uint8_t((0.5 * n.y + 0.5) * 255.0);
            n_ptr[ii + 2] = uint8_t((0.5 * n.z + 0.5) * 255.0);
        }
    }

    for (int i = 1; i < specs.chunk_size; ++i) {

    }
}

Sector::Sector(const CellKey &p_sector, HashMap<CellKey, Region*> &p_regions, const RegionSpecs &p_specs)
    : specs(p_specs)
{
    real_t nreg = (real_t)specs.sector_size / (real_t)specs.region_size;
    CellKey region0 = CellKey(p_sector.x * nreg, p_sector.z * nreg);

    if (specs.sector_regions > 1) {
        // Set covered regions.
        regions.resize(specs.sector_regions * specs.sector_regions);
        int idx = 0;

        for (size_t reg_iz = 0; reg_iz < specs.sector_regions; ++reg_iz) {
            for (size_t reg_ix = 0; reg_ix < specs.sector_regions; ++reg_ix) {
                CellKey region_key = region0 + CellKey(reg_ix, reg_iz);
                Region **region_ptr = p_regions.getptr(region_key);
                regions.write[idx] = region_ptr ? *region_ptr : nullptr;
                idx++;
            }
        }

        // Set minmax.
        minmax_buffer = (MinMax *)memalloc(specs.sector_minmax_buffer_size * sizeof(MinMax));
        idx = 0;

        for (size_t reg_iz = 0; reg_iz < specs.sector_regions; reg_iz += 2) {
            for (size_t reg_ix = 0; reg_ix < specs.sector_regions; reg_ix += 2) {
                const size_t idx00 = reg_ix + reg_iz * specs.sector_regions;
                const size_t idx10 = idx00 + 1;
                const size_t idx01 = idx00 + specs.sector_regions;
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
        MinMax *parent_minmax = minmax_buffer;
        MinMax *minmax_ptr = minmax_buffer + idx;

        for (int ilod = 1; ilod < extra_lods; ++ilod) {
            MinMax *next_parent = minmax_ptr;

            for (size_t reg_iz = 0; reg_iz < rsize; reg_iz += 2) {
                for (size_t reg_ix = 0; reg_ix < rsize; reg_ix += 2) {
                    const size_t idx00 = reg_ix + reg_iz * rsize;
                    const size_t idx10 = idx00 + 1;
                    const size_t idx01 = idx10 + rsize;
                    const size_t idx11 = idx01 + 1;
                    minmax_ptr->min = MIN(parent_minmax[idx00].min, MIN(parent_minmax[idx10].min, MIN(parent_minmax[idx01].min, parent_minmax[idx11].min)));
                    minmax_ptr->max = MAX(parent_minmax[idx00].max, MAX(parent_minmax[idx10].max, MAX(parent_minmax[idx01].max, parent_minmax[idx11].max)));
                    minmax_ptr++;
                }
            }

            rsize >>= 1;
            parent_minmax = next_parent;
        }

        // Set hmap.
        hmap_buffer = (hmap_t *)memalloc(specs.sector_hmap_buffer_size * sizeof(hmap_t));
        const int rlods = MIN(specs.chunk_lods, extra_lods);
        size_t nnodes = specs.sector_regions >> 1;
        size_t nregions = 2;
        size_t csize = specs.chunk_size >> 1;
        const size_t chunkp1 = specs.chunk_size + 1;
        const size_t node_size = chunkp1 * chunkp1 + 4 * chunkp1;

        for (int ilod = 0; ilod < rlods; ++ilod) {
            const int lod = ilod + specs.region_lods;

            for (size_t node_iz = 0; node_iz < nnodes; node_iz += nregions) {
                for (size_t node_ix = 0; node_ix < nnodes; node_ix += nregions) {
                    size_t reg0_idx = node_ix * nregions + node_iz * nregions * specs.sector_regions;
                    hmap_t *hmap_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[ilod] + (node_ix + node_iz * nnodes) * node_size;

                    for (size_t reg_iz = 0; reg_iz < nregions; ++reg_iz) {
                        hmap_t *const row_hmap_ptr = hmap_ptr;

                        for (size_t reg_ix = 0; reg_ix < nregions; ++reg_ix) {
                            size_t reg_idx = reg0_idx + reg_ix + reg_iz * specs.sector_regions;
                            Region *region = regions[reg_idx];

                            if (region) {
                                for (int i = 0; i < csize; ++i) {
                                    memcpy(hmap_ptr + i * chunkp1, region->get_hmap_chunk(lod, 0), csize * sizeof(hmap_t));
                                }
                            } else {
                                for (int i = 0; i < csize; ++i) {
                                    for (int j = 0; j < csize; ++j) {
                                        hmap_ptr[j + i * chunkp1] = specs.default_height;
                                    }
                                }
                            }

                            hmap_ptr += csize;
                        }

                        hmap_ptr = row_hmap_ptr + csize * chunkp1;
                    }
                }
            }

            csize >>= 1;
            nregions *= 2;
            nnodes >>= 1;
        }

        const size_t half_chunk = specs.chunk_size >> 1;

        for (int ilod = rlods; ilod < extra_lods; ++ilod) {
            const int lod = ilod + specs.region_lods + rlods;
            const int parent_lod = lod - 1;
            const size_t nnodes2 = 2 * nnodes;

            for (size_t node_iz = 0; node_iz < nnodes; ++node_iz) {
                for (size_t node_ix = 0; node_ix < nnodes; ++node_ix) {
                    const size_t node_idx = node_ix + node_iz * nnodes;
                    hmap_t *hmap_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[lod] + node_idx * node_size;
                    const size_t parent_idx = 2 * (node_ix + node_iz * nnodes2);
                    hmap_t *parent_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[parent_lod];
                    hmap_t *hmap_q00_ptr = parent_ptr + parent_idx * node_size;
                    hmap_t *hmap_q10_ptr = parent_ptr + (parent_idx + 1) * node_size;
                    hmap_t *hmap_q01_ptr = parent_ptr + (parent_idx + nnodes2) * node_size;
                    hmap_t *hmap_q11_ptr = parent_ptr + (parent_idx + nnodes2 + 1) * node_size;
                    idx = 0;

                    for (size_t cell_iz = 0; cell_iz < specs.chunk_size; cell_iz += 2) {
                        for (size_t cell_ix = 0; cell_ix < specs.chunk_size; cell_ix += 2) {
                            const size_t hmap_i00 = cell_ix + cell_iz * chunkp1;
                            const size_t hmap_i10 = hmap_i00 + 1;
                            const size_t hmap_i01 = hmap_i00 + chunkp1;
                            const size_t hmap_i11 = hmap_i01 + 1;
                            {
                                // Q1
                                uint32_t h = hmap_q00_ptr[hmap_i00] + hmap_q00_ptr[hmap_i10] + hmap_q00_ptr[hmap_i01] + hmap_q00_ptr[hmap_i11] + 2;
                                hmap_ptr[idx] = (hmap_t)(h >> 2);
                            }
                            {
                                // Q2
                                uint32_t h = hmap_q10_ptr[hmap_i00] + hmap_q10_ptr[hmap_i10] + hmap_q10_ptr[hmap_i01] + hmap_q10_ptr[hmap_i11] + 2;
                                hmap_ptr[idx + half_chunk] = (hmap_t)(h >> 2);
                            }
                            {
                                // Q3
                                uint32_t h = hmap_q01_ptr[hmap_i00] + hmap_q01_ptr[hmap_i10] + hmap_q01_ptr[hmap_i01] + hmap_q01_ptr[hmap_i11] + 2;
                                hmap_ptr[idx + half_chunk * chunkp1] = (hmap_t)(h >> 2);
                            }
                            {
                                // Q4
                                uint32_t h = hmap_q11_ptr[hmap_i00] + hmap_q11_ptr[hmap_i10] + hmap_q11_ptr[hmap_i01] + hmap_q11_ptr[hmap_i11] + 2;
                                hmap_ptr[idx + half_chunk * (chunkp1 + 1)] = (hmap_t)(h >> 2);
                            }
                            idx++;
                        }

                        idx += half_chunk + 1;
                    }
                }
            }

            nnodes >>= 1;
        }

        nnodes = specs.sector_regions >> 1;
        const size_t chunk_size_bytes = specs.chunk_size * sizeof(hmap_t);

        // Fill paddings.
        for (int ilod = 0; ilod < extra_lods; ++ilod) {
            const int lod = ilod + specs.region_lods;
            hmap_t *hmap_lod_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[lod];

            for (size_t node_iz = 0; node_iz < nnodes; ++node_iz) {
                for (size_t node_ix = 0; node_ix < nnodes; ++node_ix) {
                    const size_t node_idx = node_ix + node_iz * nnodes;
                    hmap_t *hmap_ptr = hmap_lod_ptr + node_idx * node_size;

                    if (node_ix != 0) {
                        hmap_t *prev_col_main = hmap_lod_ptr + (node_idx - 1) * node_size;
                        hmap_t *prev_col_ptr = prev_col_main + specs.chunk_size - 1;
                        hmap_t *left_pad_ptr = hmap_ptr + chunkp1 * chunkp1 + static_cast<size_t>(ChunkPad::X_NEG) * chunkp1;
                        hmap_t *prev_col_right_pad_ptr = prev_col_main + chunkp1 * chunkp1 + static_cast<size_t>(ChunkPad::X_POS) * chunkp1;

                        for (size_t i = 0; i < specs.chunk_size; ++i) {
                            const size_t ii = chunkp1 * i;
                            left_pad_ptr[i] = *(prev_col_ptr + ii);
                            prev_col_ptr[ii + 1] = *(hmap_ptr + ii);
                            prev_col_right_pad_ptr[i] = *(hmap_ptr + ii + 1);
                        }
                    }

                    if (node_iz != 0) {
                        hmap_t *prev_row_main = hmap_lod_ptr + (node_idx - nnodes) * node_size;
                        hmap_t *prev_row_ptr = prev_row_main + chunkp1 * (specs.chunk_size - 1);
                        hmap_t *top_pad_ptr = hmap_ptr + chunkp1 * chunkp1 + static_cast<size_t>(ChunkPad::Z_NEG) * chunkp1;
                        memcpy(top_pad_ptr, prev_row_ptr, chunk_size_bytes);
                        memcpy(prev_row_ptr + chunkp1, hmap_ptr, chunk_size_bytes);
                        memcpy(prev_row_main + chunkp1 * chunkp1 + static_cast<size_t>(ChunkPad::Z_POS) * chunkp1, hmap_ptr + chunkp1, chunk_size_bytes);

                        if (node_ix < nnodes - 1) {
                            hmap_t *prev_row_next_main = hmap_lod_ptr + (node_idx - nnodes + 1) * node_size;
                            top_pad_ptr[specs.chunk_size] = *(prev_row_next_main + chunkp1 * (specs.chunk_size - 1));
                            hmap_t *prev_row_next_left_pad_ptr = prev_row_next_main + chunkp1 * chunkp1 + static_cast<size_t>(ChunkPad::X_NEG) * chunkp1;
                            prev_row_next_left_pad_ptr[specs.chunk_size] = *(hmap_ptr + specs.chunk_size - 1);
                        }
                    }

                    if (node_ix != 0 && node_iz != 0) {
                        idx = node_idx - nnodes - 1;
                        hmap_t *prev_row_prev_main = hmap_lod_ptr + idx * node_size;
                        prev_row_prev_main[chunkp1 * chunkp1 - 1] = *hmap_ptr;
                        prev_row_prev_main[chunkp1 * chunkp1 + static_cast<size_t>(ChunkPad::X_POS) * chunkp1 + specs.chunk_size] = *(hmap_ptr + 1);
                        prev_row_prev_main[chunkp1 * chunkp1 + static_cast<size_t>(ChunkPad::Z_POS) * chunkp1 + specs.chunk_size] = *(hmap_ptr + chunkp1);
                    }
                }
            }

            nnodes >>= 1;
        }
    } else {
        regions.resize(1);
        Region **region_ptr = p_regions.getptr(region0);
        regions.write[0] = region_ptr ? *region_ptr : nullptr;
        region_offset = p_sector - CellKey(region0.x / nreg, region0.z / nreg);
    }
}

Sector::~Sector() {
    if (minmax_buffer) {
        memfree(minmax_buffer);
    }
}

