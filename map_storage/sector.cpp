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

void Sector::get_layer_data(const CellKey &p_key, int p_lod, TextureLayerData &r_layer_data) const {
    const Size node_xpd_size = specs.chunk_size + 3;
    const Size buffer_size = (specs.chunk_size + 1) * (specs.chunk_size + 1);
    const hmap_t *hmap_ptr = nullptr;
    PackedFloat32Array &heights = r_layer_data.heights;
    heights.resize(buffer_size);
    float *h_ptr = heights.ptrw();
    PackedByteArray &normals = r_layer_data.normals;
    normals.resize(4 * buffer_size);
    uint8_t *n_ptr = normals.ptrw();
    const real_t y_scale = specs.scale.y;

    if (p_lod < specs.region_lods) {
        const Size region_nodes = specs.region_size >> p_lod;
        const Size region_ix = p_key.x / region_nodes;
        const Size region_iz = p_key.z / region_nodes;
        const Size region_idx = region_ix + region_iz * specs.sector_regions;

        if (regions[region_idx]) {
            const Size node_ix = (Size)p_key.x - region_ix * region_nodes + (region_offset.x >> p_lod);
            const Size node_iz = (Size)p_key.z - region_iz * region_nodes + (region_offset.z >> p_lod);
            const Size node_idx = node_ix + node_iz * region_nodes;
            hmap_ptr = regions[region_idx]->get_hmap_node_buffer(p_lod, node_idx);
        } else {
            float h = specs.default_height * y_scale;
            heights.fill(h);

            for (int i = 0; i < buffer_size; ++i) {
                const int ii = 3 * i;
                n_ptr[ii] = 127;
                n_ptr[ii + 1] = 255;
                n_ptr[ii + 2] = 127;
            }

            return;
        }
    } else {
        const Size side = specs.sector_size >> p_lod;
        const Size block_idx = p_key.x + p_key.z * side;
        hmap_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[p_lod - specs.region_lods] + block_idx * node_xpd_size * node_xpd_size;
    }

    for (Size i = 0; i < node_xpd_size * node_xpd_size; ++i) {
        node_buffer[i] = hmap_ptr[i] * y_scale;
    }

    const float dx_inv = 1.0f / (2.0f * specs.scale.x);
    const float dz_inv = 1.0f / (2.0f * specs.scale.z);

    // Tangent can be calculated as: vec3 tangent = vec3(sqrt(1.0 - n.w * n.w), n.w, 0)
    // Bitangent can be calculated as: vec3 bitangent = cross(n.xyz, tangent)
    for (Size iz = 1; iz <= specs.chunk_size + 1; ++iz) {
        for (Size ix = 1; ix <= specs.chunk_size + 1; ++ix) {
            const Size idx = ix + iz * node_xpd_size;
            *h_ptr = node_buffer[idx];
            h_ptr++;
            const Size idx_xn = idx - 1;
            const Size idx_xp = idx + 1;
            const Size idx_zn = idx - node_xpd_size;
            const Size idx_zp = idx + node_xpd_size;
            const float dh_dx = (node_buffer[idx_xn] - node_buffer[idx_xp]) * dx_inv;
            const float dh_dz = (node_buffer[idx_zn] - node_buffer[idx_zp]) * dz_inv;
            const Vector3 n = Vector3(dh_dx, 1.0, dh_dz).normalized();
            const Size ii = 3 * idx;
            n_ptr[0] = uint8_t((0.5 * n.x + 0.5) * 255.0);
            n_ptr[1] = uint8_t((0.5 * n.y + 0.5) * 255.0);
            n_ptr[2] = uint8_t((0.5 * n.z + 0.5) * 255.0);
            const float t = dh_dx / Math::sqrt(1.0 + dh_dx * dh_dx);
            n_ptr[3] = uint8_t((0.5 * t + 0.5) * 255.0);
            n_ptr += 4;
        }
    }
}

Sector::Sector(const CellKey &p_sector, HashMap<CellKey, Region*> &p_regions, const RegionSpecs &p_specs)
    : specs(p_specs)
{
    real_t nreg = (real_t)specs.sector_size / (real_t)specs.region_size;
    CellKey region0 = CellKey(p_sector.x * nreg, p_sector.z * nreg);
    const Size chunk_size = specs.chunk_size;
    const Size node_xpd_size = chunk_size + 3;
    node_buffer = (float *)memalloc(node_xpd_size * node_xpd_size * sizeof(float));

    if (specs.sector_regions > 1) {
        // Set covered regions.
        regions.resize(specs.sector_regions * specs.sector_regions);
        Size idx = 0;

        for (uint16_t reg_iz = 0; reg_iz < specs.sector_regions; ++reg_iz) {
            for (uint16_t reg_ix = 0; reg_ix < specs.sector_regions; ++reg_ix) {
                CellKey region_key = region0 + CellKey(reg_ix, reg_iz);
                Region **region_ptr = p_regions.getptr(region_key);
                regions.write[idx] = region_ptr ? *region_ptr : nullptr;
                idx++;
            }
        }

        // Set minmax.
        minmax_buffer = (MinMax *)memalloc(specs.sector_minmax_buffer_size * sizeof(MinMax));
        idx = 0;

        for (Size reg_iz = 0; reg_iz < specs.sector_regions; reg_iz += 2) {
            for (Size reg_ix = 0; reg_ix < specs.sector_regions; reg_ix += 2) {
                const Size idx00 = reg_ix + reg_iz * specs.sector_regions;
                const Size idx10 = idx00 + 1;
                const Size idx01 = idx00 + specs.sector_regions;
                const Size idx11 = idx01 + 1;
                const MinMax mm00 = regions[idx00] ? regions[idx00]->get_minmax(specs.region_lods - 1, 0) : specs.default_minmax;
                const MinMax mm10 = regions[idx10] ? regions[idx10]->get_minmax(specs.region_lods - 1, 0) : specs.default_minmax;
                const MinMax mm01 = regions[idx01] ? regions[idx01]->get_minmax(specs.region_lods - 1, 0) : specs.default_minmax;
                const MinMax mm11 = regions[idx11] ? regions[idx11]->get_minmax(specs.region_lods - 1, 0) : specs.default_minmax;
                minmax_buffer[idx].min = MIN(mm00.min, MIN(mm10.min, MIN(mm01.min, mm11.min)));
                minmax_buffer[idx].max = MAX(mm00.max, MAX(mm10.max, MAX(mm01.max, mm11.max)));
                idx++;
            }
        }

        size_t extra_lods = specs.lods - specs.region_lods;
        Size rsize = specs.sector_regions;
        MinMax *parent_minmax = minmax_buffer;
        MinMax *minmax_ptr = minmax_buffer + idx;

        for (size_t ilod = 1; ilod < extra_lods; ++ilod) {
            MinMax *next_parent = minmax_ptr;

            for (Size reg_iz = 0; reg_iz < rsize; reg_iz += 2) {
                for (Size reg_ix = 0; reg_ix < rsize; reg_ix += 2) {
                    const Size idx00 = reg_ix + reg_iz * rsize;
                    const Size idx10 = idx00 + 1;
                    const Size idx01 = idx10 + rsize;
                    const Size idx11 = idx01 + 1;
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
        const size_t rlods = MIN(specs.chunk_lods, extra_lods);
        const size_t node_size = node_xpd_size * node_xpd_size;
        Size nnodes = specs.sector_regions >> 1;
        Size nregions = 2;
        Size csize = chunk_size >> 1;
        Size main_ptr_offset = node_xpd_size + 1;

        for (size_t ilod = 0; ilod < rlods; ++ilod) {
            const size_t lod = ilod + specs.region_lods;

            for (Size node_iz = 0; node_iz < nnodes; node_iz += nregions) {
                for (Size node_ix = 0; node_ix < nnodes; node_ix += nregions) {
                    Size reg0_idx = node_ix * nregions + node_iz * nregions * specs.sector_regions;
                    hmap_t *hmap_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[ilod] + (node_ix + node_iz * nnodes) * node_size + main_ptr_offset;

                    for (Size reg_iz = 0; reg_iz < nregions; ++reg_iz) {
                        hmap_t *const row_hmap_ptr = hmap_ptr;

                        for (Size reg_ix = 0; reg_ix < nregions; ++reg_ix) {
                            Size reg_idx = reg0_idx + reg_ix + reg_iz * specs.sector_regions;
                            Region *region = regions[reg_idx];

                            if (region) {
                                for (int i = 0; i < csize; ++i) {
                                    memcpy(hmap_ptr + i * node_xpd_size, region->get_hmap_node_buffer(lod, 0), csize * sizeof(hmap_t));
                                }
                            } else {
                                for (int i = 0; i < csize; ++i) {
                                    for (int j = 0; j < csize; ++j) {
                                        hmap_ptr[j + i * node_xpd_size] = specs.default_height;
                                    }
                                }
                            }

                            hmap_ptr += csize;
                        }

                        hmap_ptr = row_hmap_ptr + csize * node_xpd_size;
                    }
                }
            }

            csize >>= 1;
            nregions *= 2;
            nnodes >>= 1;
        }

        const Size half_chunk = chunk_size >> 1;

        for (size_t ilod = rlods; ilod < extra_lods; ++ilod) {
            const size_t lod = ilod + specs.region_lods + rlods;
            const size_t parent_lod = lod - 1;
            const Size nnodes2 = 2 * nnodes;

            for (Size node_iz = 0; node_iz < nnodes; ++node_iz) {
                for (Size node_ix = 0; node_ix < nnodes; ++node_ix) {
                    const Size node_idx = node_ix + node_iz * nnodes;
                    hmap_t *hmap_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[lod] + node_idx * node_size + main_ptr_offset;
                    const Size parent_idx = 2 * (node_ix + node_iz * nnodes2);
                    hmap_t *parent_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[parent_lod];
                    hmap_t *hmap_q00_ptr = parent_ptr + parent_idx * node_size + main_ptr_offset;
                    hmap_t *hmap_q10_ptr = parent_ptr + (parent_idx + 1) * node_size + main_ptr_offset;
                    hmap_t *hmap_q01_ptr = parent_ptr + (parent_idx + nnodes2) * node_size + main_ptr_offset;
                    hmap_t *hmap_q11_ptr = parent_ptr + (parent_idx + nnodes2 + 1) * node_size + main_ptr_offset;
                    idx = 0;

                    for (Size cell_iz = 0; cell_iz < chunk_size; cell_iz += 2) {
                        for (Size cell_ix = 0; cell_ix < chunk_size; cell_ix += 2) {
                            const Size hmap_i00 = cell_ix + cell_iz * node_xpd_size;
                            const Size hmap_i10 = hmap_i00 + 1;
                            const Size hmap_i01 = hmap_i00 + node_xpd_size;
                            const Size hmap_i11 = hmap_i01 + 1;
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
                                hmap_ptr[idx + half_chunk * node_xpd_size] = (hmap_t)(h >> 2);
                            }
                            {
                                // Q4
                                uint32_t h = hmap_q11_ptr[hmap_i00] + hmap_q11_ptr[hmap_i10] + hmap_q11_ptr[hmap_i01] + hmap_q11_ptr[hmap_i11] + 2;
                                hmap_ptr[idx + half_chunk * (node_xpd_size + 1)] = (hmap_t)(h >> 2);
                            }
                            idx++;
                        }

                        idx += half_chunk + 3;
                    }
                }
            }

            nnodes >>= 1;
        }

        nnodes = specs.sector_regions >> 1;
        const size_t chunk_size_bytes = chunk_size * sizeof(hmap_t);

        // Fill paddings.
        for (size_t ilod = 0; ilod < extra_lods; ++ilod) {
            const size_t lod = ilod + specs.region_lods;
            hmap_t *const hmap_lod_ptr = hmap_buffer + specs.sector_hmap_lod_offsets[lod];

            for (Size node_iz = 0; node_iz < nnodes; ++node_iz) {
                for (Size node_ix = 0; node_ix < nnodes; ++node_ix) {
                    const Size node_idx = node_ix + node_iz * nnodes;
                    hmap_t *hmap_node_buffer = hmap_lod_ptr + node_idx * node_size;
                    hmap_t *hmap_ptr = hmap_node_buffer + node_xpd_size + 1;

                    if (node_ix != 0) {
                        hmap_t *prev_col = hmap_lod_ptr + (node_idx - 1) * node_size + node_xpd_size + chunk_size;
                        hmap_t *left_pad = hmap_node_buffer + node_xpd_size;

                        for (Size i = 0; i < chunk_size; ++i) {
                            const Size ii = i * node_xpd_size;
                            left_pad[ii] = prev_col[ii];
                            prev_col[ii + 1] = hmap_ptr[ii];
                            prev_col[ii + 2] = hmap_ptr[ii + 1];
                        }
                    }

                    if (node_iz != 0) {
                        hmap_t *prev_row = hmap_lod_ptr + (node_idx - rsize) * node_size + node_xpd_size * chunk_size + 1;
                        hmap_t *top_pad = hmap_node_buffer + 1;
                        memcpy(top_pad, prev_row, chunk_size_bytes);
                        memcpy(prev_row + node_xpd_size, hmap_ptr, chunk_size_bytes);
                        memcpy(prev_row + 2 * node_xpd_size, hmap_ptr + node_xpd_size, chunk_size_bytes);

                        if (node_ix < nnodes - 1) {
                            hmap_t *prev_row_next = hmap_lod_ptr + (node_idx - rsize + 1) * node_size;
                            top_pad[chunk_size] = prev_row_next[node_xpd_size * chunk_size + 1];
                            prev_row_next[node_xpd_size * chunk_size] = hmap_ptr[chunk_size - 1];
                        }
                    }

                    if (node_ix != 0 && node_iz != 0) {
                        hmap_t *prev_row_prev = hmap_lod_ptr + (node_idx - rsize - 1) * node_size + node_xpd_size * (node_xpd_size - 1) - 2;
                        prev_row_prev[0] = hmap_ptr[0];
                        prev_row_prev[1] = hmap_ptr[1];
                        prev_row_prev[node_xpd_size] = hmap_ptr[node_xpd_size];
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

    if (node_buffer) {
        memfree(node_buffer);
    }
}

