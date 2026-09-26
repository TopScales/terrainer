/**
 * region.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#include "region.h"

using namespace Terrainer;

bool Region::load() {
    HeaderInfo info;
    access->get_buffer((uint8_t *)&info, FILE_HEADER_INFO_SIZE);

    for (size_t i = 0; i < MAGIC_SIZE; ++i) {
        ERR_FAIL_COND_V_EDMSG(info.magic[i] != MAGIC_STRING[i], false, vformat("Region file %s has incorrect format.", access->get_path().get_file()));
    }

    ERR_FAIL_COND_V_EDMSG(info.chunk_size != specs.chunk_size, false, vformat("Wrong chunk size in region file %s.", access->get_path().get_file()));
    ERR_FAIL_COND_V_EDMSG(info.region_size != specs.region_size, false, vformat("Wrong region size in region file %s.", access->get_path().get_file()));
    ERR_FAIL_COND_V_EDMSG(info.version > specs.version, false, vformat("Unsupported file version in region file %s.", access->get_path().get_file()));
    format_mismatch = info.format != specs.format || info.chunk_lods != specs.chunk_lods;
    const Size buffer_size = specs.get_buffer_size();
    ERR_FAIL_COND_V_EDMSG(FileAccess::get_size(access->get_path_absolute()) != buffer_size + FILE_HEADER_INFO_SIZE, false, vformat("Incorrect file size for region file %s", access->get_path().get_file()));
    buffer = (uint8_t *)memalloc(buffer_size);
    access->get_buffer(buffer, buffer_size);
    minmax_buffer = (MinMax *)buffer;
    hmap_buffer = (hmap_t *)(buffer + specs.get_minmax_buffer_size() * sizeof(MinMax));
    return true;
}

void Region::load_hmap_region(const CellKey &p_region, const CellKey &p_regions, const PackedByteArray &p_data, const Vector2i &p_size) {
    const Size buffer_size = specs.get_buffer_size();
    const Size chunk_size = specs.chunk_size;
    const Size region_size = specs.region_size;
    const Size region_cells = region_size * chunk_size;
    buffer = (uint8_t *)memalloc(buffer_size);
    minmax_buffer = (MinMax *)buffer;
    hmap_buffer = (hmap_t *)(buffer + specs.get_minmax_buffer_size() * sizeof(MinMax));
    MinMax *minmax_ptr = minmax_buffer;
    const Size node_xpd_size = chunk_size + 3;

    for (Size chunk_iz = 0; chunk_iz < region_size; ++chunk_iz) {
        for (Size chunk_ix = 0; chunk_ix < region_size; ++chunk_ix) {
            uint8_t min_h = UINT8_MAX;
            uint8_t max_h = 0;
            const Size chunk_idx = chunk_ix + chunk_iz * region_size;
            hmap_t *hmap_ptr = get_hmap_node_buffer(0, chunk_idx);

            // Set chunk's heights.
            for (Size cell_iz = 0; cell_iz < node_xpd_size; ++cell_iz) {
                const Size data_z = CLAMP(cell_iz - 1 + chunk_iz * chunk_size + p_region.z * region_cells, 0, p_size.y - 1);

                for (Size cell_ix = 0; cell_ix < node_xpd_size; ++cell_ix) {
                    const Size data_x = CLAMP(cell_ix - 1 + chunk_ix * chunk_size + p_region.x * region_cells, 0, p_size.x - 1);
                    const Size data_idx = data_x + data_z * p_size.x;
                    const uint8_t h = p_data[data_idx];
                    min_h = MIN(min_h, h);
                    max_h = MAX(max_h, h);
                    *hmap_ptr = h;
                    hmap_ptr++;
                }
            }

            // Set minmax for this chunk.
            minmax_ptr->min = min_h;
            minmax_ptr->max = max_h;
            minmax_ptr++;
        }
    }

    Size rsize = region_size >> 1;
    MinMax *prev_minmax = minmax_buffer;
    const size_t half_chunk = chunk_size >> 1;
    const size_t chunk_size_bytes = chunk_size * sizeof(hmap_t);

    for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
        MinMax *next_minmax = minmax_ptr;
        const Size rsize2 = 2 * rsize;

        for (Size node_iz = 0; node_iz < rsize; ++node_iz) {
            for (Size node_ix = 0; node_ix < rsize; ++node_ix) {
                const Size parent_idx = 2 * (node_ix + node_iz * rsize2);
                const Size minmax_i00 = parent_idx;
                const Size minmax_i10 = parent_idx + 1;
                const Size minmax_i01 = parent_idx + rsize2;
                const Size minmax_i11 = minmax_i01 + 1;
                const MinMax minmax00 = prev_minmax[minmax_i00];
                const MinMax minmax10 = prev_minmax[minmax_i10];
                const MinMax minmax01 = prev_minmax[minmax_i01];
                const MinMax minmax11 = prev_minmax[minmax_i11];
                const hmap_t hmin = MIN(minmax00.min, MIN(minmax10.min, MIN(minmax01.min, minmax11.min)));
                const hmap_t hmax = MAX(minmax00.max, MAX(minmax10.max, MAX(minmax01.max, minmax11.max)));
                minmax_ptr->min = hmin;
                minmax_ptr->max = hmax;
                minmax_ptr++;
                Size node_idx = node_ix + node_iz * rsize;
                hmap_t *hmap_node_buffer = get_hmap_node_buffer(ilod, node_idx);
                hmap_t *hmap_ptr = hmap_node_buffer + node_xpd_size + 1;
                hmap_t *hmap_q00_ptr = get_hmap_node_main(ilod - 1, parent_idx);
                hmap_t *hmap_q10_ptr = get_hmap_node_main(ilod - 1, parent_idx + 1);
                hmap_t *hmap_q01_ptr = get_hmap_node_main(ilod - 1, parent_idx + rsize2);
                hmap_t *hmap_q11_ptr = get_hmap_node_main(ilod - 1, parent_idx + rsize2 + 1);
                size_t idx = 0;

                for (Size cell_iz = 0; cell_iz < chunk_size; cell_iz += 2) {
                    for (Size cell_ix = 0; cell_ix < chunk_size; cell_ix += 2) {
                        const size_t hmap_i00 = cell_ix + cell_iz * node_xpd_size;
                        const size_t hmap_i10 = hmap_i00 + 1;
                        const size_t hmap_i01 = hmap_i00 + node_xpd_size;
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

                // Fill paddings.
                if (node_ix != 0) {
                    hmap_t *prev_col = get_hmap_node_buffer(ilod, node_idx - 1) + node_xpd_size + chunk_size;
                    hmap_t *left_pad = hmap_node_buffer + node_xpd_size;

                    for (Size i = 0; i < chunk_size; ++i) {
                        const Size ii = i * node_xpd_size;
                        left_pad[ii] = prev_col[ii];
                        prev_col[ii + 1] = hmap_ptr[ii];
                        prev_col[ii + 2] = hmap_ptr[ii + 1];
                    }
                }

                if (node_iz != 0) {
                    hmap_t *prev_row = get_hmap_node_buffer(ilod, node_idx - rsize) + node_xpd_size * chunk_size + 1;
                    hmap_t *top_pad = hmap_node_buffer + 1;
                    memcpy(top_pad, prev_row, chunk_size_bytes);
                    memcpy(prev_row + node_xpd_size, hmap_ptr, chunk_size_bytes);
                    memcpy(prev_row + 2 * node_xpd_size, hmap_ptr + node_xpd_size, chunk_size_bytes);

                    if (node_ix < rsize - 1) {
                        hmap_t *prev_row_next = get_hmap_node_buffer(ilod, node_idx - rsize + 1);
                        top_pad[chunk_size] = prev_row_next[node_xpd_size * chunk_size + 1];
                        prev_row_next[node_xpd_size * chunk_size] = hmap_ptr[chunk_size - 1];
                    }
                }

                if (node_ix != 0 && node_iz != 0) {
                    hmap_t *prev_row_prev = get_hmap_node_buffer(ilod, node_idx - rsize - 1) + node_xpd_size * (node_xpd_size - 1) - 2;
                    prev_row_prev[0] = hmap_ptr[0];
                    prev_row_prev[1] = hmap_ptr[1];
                    prev_row_prev[node_xpd_size] = hmap_ptr[node_xpd_size];
                }
            }
        }

        prev_minmax = next_minmax;
        rsize >>= 1;
    }

    // Chunk LODs.
    Size csize = chunk_size;
    Size node_size = node_xpd_size;
    hmap_t *parent_ptr = get_hmap_node_main(specs.region_lods - 1, 0);

    for (size_t ilod = specs.region_lods; ilod < specs.region_lods + specs.chunk_lods; ++ilod) {
        hmap_t *hmap_ptr = get_hmap_node_buffer(ilod, 0);
        Size idx = 0;

        for (Size cell_iz = 0; cell_iz < csize; cell_iz += 2) {
            for (Size cell_ix = 0; cell_ix < csize; cell_ix += 2) {
                const Size hmap_i00 = cell_ix + cell_iz * node_size;
                const Size hmap_i10 = hmap_i00 + 1;
                const Size hmap_i01 = hmap_i00 + node_size;
                const Size hmap_i11 = hmap_i01 + 1;
                uint32_t h = parent_ptr[hmap_i00] + parent_ptr[hmap_i10] + parent_ptr[hmap_i01] + parent_ptr[hmap_i11] + 2;
                hmap_ptr[idx] = (hmap_t)(h >> 2);
                idx++;
            }
        }

        parent_ptr = hmap_ptr;
        csize >>= 1;
        node_size = csize;
    }

    // Fill pads for border regions.
    if (p_region.x == 0) {
        rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (Size inode = 0; inode < rsize; ++inode) {
                hmap_t *left_pad = get_hmap_node_buffer(ilod, inode * rsize) + node_xpd_size;

                for (Size i = 0; i <= chunk_size; ++i) {
                    const Size ii = i * node_xpd_size;
                    left_pad[ii] = left_pad[ii + 1];
                }
            }

            rsize >>= 1;
        }
    }

    if (p_region.z == 0) {
        rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (Size inode = 0; inode < rsize; ++inode) {
                hmap_t *top_pad = get_hmap_node_buffer(ilod, inode) + 1;
                memcpy(top_pad, top_pad + node_xpd_size, (chunk_size + 1) * sizeof(hmap_t));
            }

            rsize >>= 1;
        }
    }

    if (p_region.x == p_regions.x - 1) {
        rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (Size inode = 0; inode < rsize; ++inode) {
                const Size node_idx = (inode + 1) * rsize - 1;
                hmap_t *right_col = get_hmap_node_buffer(ilod, node_idx) + chunk_size;

                for (Size i = 0; i <= chunk_size; ++i) {
                    const Size ii = i * node_xpd_size;
                    right_col[ii + 1] = right_col[ii];
                    right_col[ii + 2] = right_col[ii];
                }
            }

            rsize >>= 1;
        }

    }

    if (p_region.z == p_regions.z - 1) {
        rsize = region_size >> 1;
        size_t nbytes = (node_size + 1) * sizeof(hmap_t);

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (Size inode = 0; inode < rsize; ++inode) {
                Size node_idx = inode + rsize * (rsize - 1);
                hmap_t *bottom_row = get_hmap_node_buffer(ilod, node_idx) + node_xpd_size * chunk_size;
                memcpy(bottom_row + node_xpd_size, bottom_row, nbytes);
                memcpy(bottom_row + 2 * node_xpd_size, bottom_row, nbytes);
            }

            rsize >>= 1;
        }
    }
}

void Region::fill_hmap_region_pad(const CellKey &p_region, const CellKey &p_regions, Vector<Region *> &p_regions_pool, int p_pool_index) {
    const Size pool_size = p_regions_pool.size();
    const Size chunk_size = specs.chunk_size;
    const Size region_size = specs.region_size;
    const Size node_xpd_size = chunk_size + 3;

    if (p_region.x != 0) {
        const Size prev_col_idx = (p_pool_index + pool_size - 1) % pool_size;
        Region *prev_col_region = p_regions_pool[prev_col_idx];
        Size rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (Size inode = 0; inode < rsize; ++inode) {
                const Size node_idx = inode * rsize;
                const Size prev_col_node_idx = node_idx + rsize - 1;
                hmap_t *left_pad = get_hmap_node_buffer(ilod, node_idx) + node_xpd_size;
                hmap_t *prev_col = prev_col_region->get_hmap_node_main(ilod, prev_col_node_idx) + chunk_size - 1;

                for (Size i = 0; i <= chunk_size; ++i) {
                    Size ii = i * node_xpd_size;
                    left_pad[ii] = prev_col[ii];
                    prev_col[ii + 1] = left_pad[ii + 1];
                    prev_col[ii + 2] = left_pad[ii + 2];
                }
            }

            rsize >>= 1;
        }
    }

    if (p_region.z != 0) {
        const Size prev_row_idx = (p_pool_index + 2) % pool_size;
        Region *prev_row_region = p_regions_pool[prev_row_idx];
        Size rsize = region_size >> 1;
        const size_t nbytes = (chunk_size + 1) * sizeof(hmap_t);

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (Size inode = 0; inode < rsize; ++inode) {
                hmap_t *top_pad = get_hmap_node_buffer(ilod, inode) + 1;
                const Size prev_row_node_idx = inode + rsize * (rsize - 1);
                hmap_t *prev_row = prev_row_region->get_hmap_node_buffer(ilod, prev_row_node_idx) + node_xpd_size * chunk_size + 1;
                memcpy(top_pad, prev_row, nbytes);
                memcpy(prev_row + node_xpd_size, top_pad + node_xpd_size, nbytes);
                memcpy(prev_row + 2 * node_xpd_size, top_pad + 2 * node_xpd_size, nbytes);
            }

            rsize >>= 1;
        }

        if (p_region.x != 0) {
            const Size prev_row_prev_idx = (p_pool_index + 1) % pool_size;
            Region *prev_row_prev_region = p_regions_pool[prev_row_prev_idx];
            rsize = region_size >> 1;

            for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
                const hmap_t *main = get_hmap_node_main(ilod, 0);
                const Size corner_idx = rsize * rsize - 1;
                hmap_t *corner = prev_row_prev_region->get_hmap_node_buffer(ilod, corner_idx) + node_xpd_size * (node_xpd_size - 1) - 2;
                corner[0] = main[0];
                corner[1] = main[1];
                corner[node_xpd_size] = main[node_xpd_size];
                rsize >>= 1;
            }
        }

        if (p_region.x < p_regions.x - 1) {
            const Size prev_row_next_idx = (p_pool_index + 3) % pool_size;
            Region *prev_row_next_region = p_regions_pool[prev_row_next_idx];
            rsize = region_size >> 1;

            for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
                hmap_t *corner_prev_row_next = prev_row_next_region->get_hmap_node_buffer(ilod, rsize * (rsize - 1)) + node_xpd_size * (node_xpd_size - 3);
                hmap_t *top_pad = get_hmap_node_buffer(ilod, rsize - 1) + chunk_size;
                corner_prev_row_next[node_xpd_size] = top_pad[node_xpd_size];
                top_pad[1] = corner_prev_row_next[1];
                rsize >>= 1;
            }
        }
    }
}

void Region::store_hmap() const {
    write_header();
    access->store_buffer(buffer, specs.get_buffer_size());
}

PackedInt32Array Region::get_node_hmap_values(size_t p_lod, const CellKey &p_node) const {
    // TODO: Make sure hmap is loaded.
    ERR_FAIL_INDEX_V_EDMSG(p_lod, specs.region_lods, PackedInt32Array(), "LOD out of range.");
    const Size region_side = specs.region_size >> p_lod;
    ERR_FAIL_INDEX_V_EDMSG(p_node.x, region_side, PackedInt32Array(), vformat("Chunk x index (%d) out of range (%d).", p_node.x, region_side));
    ERR_FAIL_INDEX_V_EDMSG(p_node.z, region_side, PackedInt32Array(), vformat("Chunk z index (%d) out of range (%d).", p_node.z, region_side));
    const Size node_idx = p_node.x + p_node.z * region_side;
    const hmap_t *hmap_ptr = get_hmap_node_main(p_lod, node_idx);
    PackedInt32Array values;
    Size size = specs.chunk_size * specs.chunk_size;
    values.resize(size);
    int *values_ptr = values.ptrw();

    for (int iz = 0; iz < specs.chunk_size; ++iz) {
        for (int ix = 0; ix < specs.chunk_size; ++ix) {
            *values_ptr = (int)*hmap_ptr;
            values_ptr++;
            hmap_ptr++;
        }

        hmap_ptr += 3;
    }

    return values;
}

void Region::write_header() const {
    HeaderInfo info{};
    info.magic[0] = MAGIC_STRING[0];
    info.magic[1] = MAGIC_STRING[1];
    info.magic[2] = MAGIC_STRING[2];
    info.magic[3] = MAGIC_STRING[3];
    info.version = specs.version;
    info.format = specs.format;
    info.region_lods = specs.region_lods;
    info.chunk_lods = specs.chunk_lods;
    info.chunk_size = specs.chunk_size;
    info.region_size = specs.region_size;
    access->store_buffer((uint8_t *)&info, FILE_HEADER_INFO_SIZE);
}

Region::hmap_t *Region::get_hmap_node_buffer(size_t p_lod, Size p_node_idx) const {
#ifdef TERRAINER_DEBUG
    ERR_FAIL_COND_V_EDMSG(p_lod >= specs.region_lods + specs.chunk_lods, nullptr, vformat("LOD %d out of range (%d).", p_lod, specs.region_lods + specs.chunk_lods));
    const Size max_nodes = p_lod < specs.region_lods ? (specs.region_size >> p_lod) * (specs.region_size >> p_lod) : 1;
    ERR_FAIL_INDEX_V_EDMSG(p_node_idx, max_nodes, nullptr, "Node index out of range.");
#endif
    Size node_size = specs.hmap_lod_node_sizes[p_lod];
    return hmap_buffer + specs.hmap_lod_offsets[p_lod] + node_size * p_node_idx;
}

Region::hmap_t *Region::get_hmap_node_main(size_t p_lod, Size p_node_idx) const {
#ifdef TERRAINER_DEBUG
    ERR_FAIL_COND_V_EDMSG(p_lod >= specs.region_lods, nullptr, vformat("LOD %d out of range (%d).", p_lod, specs.region_lods));
    ERR_FAIL_INDEX_V_EDMSG(p_node_idx, (specs.region_size >> p_lod) * (specs.region_size >> p_lod), nullptr, "Node index out of range.");
#endif
    Size node_size = specs.hmap_lod_node_sizes[p_lod];
    return hmap_buffer + specs.hmap_lod_offsets[p_lod] + node_size * p_node_idx + specs.chunk_size + 4;
}

// Region::hmap_t *Region::get_hmap_chunk(size_t p_lod, Size p_chunk_idx) const {
//     Size chunk_size = specs.hmap_lod_node_sizes[p_lod];
//     return hmap_buffer + specs.hmap_lod_offsets[p_lod] + chunk_size * p_chunk_idx;
// }

// Region::hmap_t *Region::get_hmap_chunk_pad(size_t p_lod, size_t p_chunk_idx, ChunkPad p_pad) const {
//     size_t chunk_size = specs.hmap_lod_node_sizes[p_lod];
//     size_t chunkp1 = specs.chunk_size + 1;
//     return hmap_buffer + specs.hmap_lod_offsets[p_lod] + chunk_size * p_chunk_idx + chunkp1 * chunkp1 + static_cast<size_t>(p_pad) * chunkp1;
// }

Region::Region(const Specs &p_specs, const Ref<FileAccess> &p_access)
    : specs(p_specs), access(p_access)
{

}

Region::~Region() {
    if (buffer) {
        memfree(buffer);
        buffer = nullptr;
        minmax_buffer = nullptr;
        hmap_buffer = nullptr;
    }
}