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

    for (int i = 0; i < MAGIC_SIZE; ++i) {
        ERR_FAIL_COND_V_EDMSG(info.magic[i] != MAGIC_STRING[i], false, vformat("Region file %s has incorrect format.", access->get_path().get_file()));
    }

    ERR_FAIL_COND_V_EDMSG(info.chunk_size != specs.chunk_size, false, vformat("Wrong chunk size in region file %s.", access->get_path().get_file()));
    ERR_FAIL_COND_V_EDMSG(info.region_size != specs.region_size, false, vformat("Wrong region size in region file %s.", access->get_path().get_file()));
    ERR_FAIL_COND_V_EDMSG(info.version > specs.version, false, vformat("Unsupported file version in region file %s.", access->get_path().get_file()));
    format_mismatch = info.format != specs.format || info.chunk_lods != specs.chunk_lods;
    const size_t buffer_size = specs.get_buffer_size();
    ERR_FAIL_COND_V_EDMSG(FileAccess::get_size(access->get_path_absolute()) != buffer_size + FILE_HEADER_INFO_SIZE, false, vformat("Incorrect file size for region file %s", access->get_path().get_file()));
    buffer = (uint8_t *)memalloc(buffer_size);
    access->get_buffer(buffer, buffer_size);
    minmax_buffer = (MinMax *)buffer;
    hmap_buffer = (hmap_t *)(buffer + specs.get_minmax_buffer_size() * sizeof(MinMax));
    return true;
}

void Region::load_hmap_region(const CellKey &p_region, const CellKey &p_regions, const PackedByteArray &p_data, const Vector2i &p_size) {
    const int64_t buffer_size = specs.get_buffer_size();
    const int32_t chunk_size = specs.chunk_size;
    const int32_t region_size = specs.region_size;
    const int32_t region_cells = region_size * chunk_size;
    buffer = (uint8_t *)memalloc(buffer_size);
    minmax_buffer = (MinMax *)buffer;
    hmap_buffer = (hmap_t *)(buffer + specs.get_minmax_buffer_size() * sizeof(MinMax));
    const int32_t chunkp1 = chunk_size + 1;
    MinMax *minmax_ptr = minmax_buffer;

    for (int32_t chunk_iz = 0; chunk_iz < region_size; ++chunk_iz) {
        for (int32_t chunk_ix = 0; chunk_ix < region_size; ++chunk_ix) {
            uint8_t min_h = UINT8_MAX;
            uint8_t max_h = 0;
            const size_t chunk_idx = chunk_ix + chunk_iz * region_size;
            hmap_t *hmap_ptr = get_hmap_chunk(0, chunk_idx);

            // Set chunk's heights.
            for (size_t cell_iz = 0; cell_iz <= chunk_size; ++cell_iz) {
                const size_t z = MIN(cell_iz + chunk_iz * chunk_size + p_region.z * region_cells, p_size.y - 1);

                for (size_t cell_ix = 0; cell_ix <= chunk_size; ++cell_ix) {
                    const size_t x = MIN(cell_ix + chunk_ix * chunk_size + p_region.x * region_cells, p_size.x - 1);
                    const size_t index = x + z * p_size.x;
                    const uint8_t h = p_data[index];
                    min_h = MIN(min_h, h);
                    max_h = MAX(max_h, h);
                    *hmap_ptr = h;
                    hmap_ptr++;
                }
            }

            // Fill LOD0 padding.
            const int64_t ixneg = MAX(chunk_ix * chunk_size + p_region.x * region_cells - 1, 0);
            const int64_t ixpos = MIN((chunk_ix + 1) * chunk_size + p_region.x * region_cells + 1, p_size.x - 1);
            const int64_t izneg = MAX(chunk_iz * chunk_size + p_region.z * region_cells - 1, 0);
            const int64_t izpos = MIN((chunk_iz + 1) * chunk_size + p_region.z * region_cells + 1, p_size.y - 1);

            for (int i = 0; i <= chunk_size; ++i) {
                const int64_t z_x = MIN(i + chunk_iz * chunk_size + p_region.z * region_cells, p_size.y - 1);
                hmap_ptr[i] = p_data[ixneg + z_x * p_size.x];
                hmap_ptr[i + chunkp1] = p_data[ixpos + z_x * p_size.x];
                const int64_t x_z = MIN(i + chunk_ix * chunk_size + p_region.x * region_cells, p_size.x - 1);
                hmap_ptr[i + 2 * chunkp1] = p_data[x_z + izneg * p_size.x];
                hmap_ptr[i + 3 * chunkp1] = p_data[x_z + izpos * p_size.x];
            }

            // Set minmax for this chunk.
            minmax_ptr->min = min_h;
            minmax_ptr->max = max_h;
            minmax_ptr++;
        }
    }

    size_t rsize = region_size >> 1;
    MinMax *prev_minmax = minmax_buffer;
    size_t half_chunk = chunk_size >> 1;

    for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
        MinMax *next_minmax = minmax_ptr;
        size_t rsize2 = 2 * rsize;

        for (size_t chunk_iz = 0; chunk_iz < rsize; ++chunk_iz) {
            for (size_t chunk_ix = 0; chunk_ix < rsize; ++chunk_ix) {
                const size_t parent_idx = 2 * (chunk_ix + chunk_iz * rsize2);
                const size_t minmax_i00 = parent_idx;
                const size_t minmax_i10 = parent_idx + 1;
                const size_t minmax_i01 = parent_idx + rsize2;
                const size_t minmax_i11 = minmax_i01 + 1;
                const MinMax minmax00 = prev_minmax[minmax_i00];
                const MinMax minmax10 = prev_minmax[minmax_i10];
                const MinMax minmax01 = prev_minmax[minmax_i01];
                const MinMax minmax11 = prev_minmax[minmax_i11];
                const hmap_t hmin = MIN(minmax00.min, MIN(minmax10.min, MIN(minmax01.min, minmax11.min)));
                const hmap_t hmax = MAX(minmax00.max, MAX(minmax10.max, MAX(minmax01.max, minmax11.max)));
                minmax_ptr->min = hmin;
                minmax_ptr->max = hmax;
                minmax_ptr++;
                size_t chunk_idx = chunk_ix + chunk_iz * rsize;
                hmap_t *hmap_ptr = get_hmap_chunk(ilod, chunk_idx);
                hmap_t *hmap_q00_ptr = get_hmap_chunk(ilod - 1, parent_idx);
                hmap_t *hmap_q10_ptr = get_hmap_chunk(ilod - 1, parent_idx + 1);
                hmap_t *hmap_q01_ptr = get_hmap_chunk(ilod - 1, parent_idx + rsize2);
                hmap_t *hmap_q11_ptr = get_hmap_chunk(ilod - 1, parent_idx + rsize2 + 1);
                size_t idx = 0;

                for (size_t cell_iz = 0; cell_iz < chunk_size; cell_iz += 2) {
                    for (size_t cell_ix = 0; cell_ix < chunk_size; cell_ix += 2) {
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

                // Fill paddings.
                if (chunk_ix != 0) {
                    hmap_t *prev_col_ptr = get_hmap_chunk(ilod, chunk_idx - 1) + chunk_size - 1;
                    hmap_t *left_pad_ptr = get_hmap_chunk_pad(ilod, chunk_idx, ChunkPad::X_NEG);
                    hmap_t *prev_col_right_pad_ptr = get_hmap_chunk_pad(ilod, chunk_idx - 1, ChunkPad::X_POS);

                    for (size_t i = 0; i < chunk_size; ++i) {
                        const size_t ii = chunkp1 * i;
                        left_pad_ptr[i] = *(prev_col_ptr + ii);
                        prev_col_ptr[ii + 1] = *(hmap_ptr + ii);
                        prev_col_right_pad_ptr[i] = *(hmap_ptr + ii + 1);
                    }
                }

                if (chunk_iz != 0) {
                    hmap_t *prev_row_ptr = get_hmap_chunk(ilod, chunk_idx - rsize) + chunkp1 * (chunk_size - 1);
                    hmap_t *top_pad_ptr = get_hmap_chunk_pad(ilod, chunk_idx, ChunkPad::Z_NEG);
                    memcpy(top_pad_ptr, prev_row_ptr, chunk_size);
                    memcpy(prev_row_ptr + chunkp1, hmap_ptr, chunk_size);
                    memcpy(get_hmap_chunk_pad(ilod, chunk_idx - rsize, ChunkPad::Z_POS), hmap_ptr + chunkp1, chunk_size);

                    if (chunk_ix < rsize - 1) {
                        hmap_t *prev_row_next_ptr = get_hmap_chunk(ilod, chunk_idx - rsize + 1);
                        top_pad_ptr[chunk_size] = *(prev_row_next_ptr + chunkp1 * (chunk_size - 1));
                        hmap_t *prev_row_next_left_pad_ptr = get_hmap_chunk_pad(ilod, chunk_idx - rsize + 1, ChunkPad::X_NEG);
                        prev_row_next_left_pad_ptr[chunk_size] = *(hmap_ptr + chunk_size - 1);
                    }
                }

                if (chunk_ix != 0 && chunk_iz != 0) {
                    idx = chunk_idx - rsize - 1;
                    get_hmap_chunk(ilod, idx)[chunkp1 * chunkp1 - 1] = *hmap_ptr;
                    get_hmap_chunk_pad(ilod, idx, ChunkPad::X_POS)[chunk_size] = *(hmap_ptr + 1);
                    get_hmap_chunk_pad(ilod, idx, ChunkPad::Z_POS)[chunk_size] = *(hmap_ptr + chunkp1);
                }
            }
        }

        prev_minmax = next_minmax;
        rsize >>= 1;
    }

    size_t csize = chunk_size + 1;

    for (size_t ilod = specs.region_lods; ilod < specs.region_lods + specs.chunk_lods; ++ilod) {
        hmap_t *parent_ptr = get_hmap_chunk(ilod - 1, 0);
        hmap_t *hmap_ptr = get_hmap_chunk(ilod, 0);
        size_t idx = 0;

        for (size_t cell_iz = 0; cell_iz < csize - 1; cell_iz += 2) {
            for (size_t cell_ix = 0; cell_ix < csize - 1; cell_ix += 2) {
                const size_t hmap_i00 = cell_ix + cell_iz * csize;
                const size_t hmap_i10 = hmap_i00 + 1;
                const size_t hmap_i01 = hmap_i00 + csize;
                const size_t hmap_i11 = hmap_i01 + 1;
                uint32_t h = parent_ptr[hmap_i00] + parent_ptr[hmap_i10] + parent_ptr[hmap_i01] + parent_ptr[hmap_i11] + 2;
                hmap_ptr[idx] = (hmap_t)(h >> 2);
                idx++;
            }
        }

        csize >>= 1;
    }

    if (p_region.x == 0) {
        rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (size_t ichunk = 0; ichunk < rsize; ++ichunk) {
                hmap_t *left_pad = get_hmap_chunk_pad(ilod, ichunk * rsize, ChunkPad::X_NEG);
                const hmap_t *main_ptr = get_hmap_chunk(ilod, ichunk * rsize);

                for (size_t i = 0; i <= chunk_size; ++i) {
                    left_pad[i] = main_ptr[i * chunkp1];
                }
            }

            rsize >>= 1;
        }
    }

    if (p_region.z == 0) {
        rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (size_t ichunk = 0; ichunk < rsize; ++ichunk) {
                hmap_t *top_pad = get_hmap_chunk_pad(ilod, ichunk, ChunkPad::Z_NEG);
                const hmap_t *main_ptr = get_hmap_chunk(ilod, ichunk);
                memcpy(top_pad, main_ptr, chunkp1);
            }

            rsize >>= 1;
        }
    }

    if (p_region.x == p_regions.x - 1) {
        rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (size_t ichunk = 0; ichunk < rsize; ++ichunk) {
                size_t chunk_idx = (ichunk + 1) * rsize - 1;
                hmap_t *main_ptr = get_hmap_chunk(ilod, chunk_idx) + chunk_size - 1;
                hmap_t *right_pad_ptr = get_hmap_chunk_pad(ilod, chunk_idx, ChunkPad::X_POS);

                for (size_t i = 0; i <= chunk_size; ++i) {
                    main_ptr[1] = *main_ptr;
                    right_pad_ptr[i] = *main_ptr;
                    main_ptr += chunkp1;
                }
            }

            rsize >>= 1;
        }

    }

    if (p_region.z == p_regions.z - 1) {
        rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (size_t ichunk = 0; ichunk < rsize; ++ichunk) {
                size_t chunk_idx = ichunk + rsize * (rsize - 1);
                hmap_t *main_ptr = get_hmap_chunk(ilod, chunk_idx) + chunkp1 * (chunk_size - 1);
                memcpy(main_ptr + chunkp1, main_ptr, chunkp1);
                memcpy(get_hmap_chunk_pad(ilod, chunk_idx, ChunkPad::Z_POS), main_ptr, chunkp1);
            }

            rsize >>= 1;
        }
    }
}

void Region::fill_hmap_region_pad(const CellKey &p_region, const CellKey &p_regions, Vector<Region *> &p_regions_pool, int p_pool_index) {
    const size_t pool_size = p_regions_pool.size();
    const size_t chunk_size = specs.chunk_size;
    const size_t region_size = specs.region_size;
    const size_t chunkp1 = chunk_size + 1;

    if (p_region.x != 0) {
        const size_t prev_col_idx = (p_pool_index + pool_size - 1) % pool_size;
        Region *prev_col_region = p_regions_pool[prev_col_idx];
        size_t rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (size_t ichunk = 0; ichunk < rsize; ++ichunk) {
                size_t chunk_idx = ichunk * rsize;
                size_t prev_col_chunk_idx = chunk_idx + rsize - 1;
                hmap_t *left_pad = get_hmap_chunk_pad(ilod, chunk_idx, ChunkPad::X_NEG);
                hmap_t *prev_col_ptr = prev_col_region->get_hmap_chunk(ilod, prev_col_chunk_idx) + chunk_size - 1;
                const hmap_t *main_ptr = get_hmap_chunk(ilod, chunk_idx);
                hmap_t *prev_col_right_pad_ptr = prev_col_region->get_hmap_chunk_pad(ilod, prev_col_chunk_idx, ChunkPad::X_POS);

                for (size_t i = 0; i <= chunk_size; ++i) {
                    size_t ii = i * chunkp1;
                    left_pad[i] = *prev_col_ptr + ii;
                    prev_col_ptr[ii + 1] = *main_ptr + ii;
                    prev_col_right_pad_ptr[i] = *main_ptr + ii + 1;
                }
            }

            rsize >>= 1;
        }

        if (p_region.z != 0) {
            const size_t prev_row_prev_idx = (p_pool_index + 1) % pool_size;
            Region *prev_row_prev_region = p_regions_pool[prev_row_prev_idx];
            rsize = region_size >> 1;

            for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
                const hmap_t *main = get_hmap_chunk(ilod, 0);
                const size_t corner_idx = rsize * rsize - 1;
                hmap_t *corner_ptr = prev_row_prev_region->get_hmap_chunk(ilod, corner_idx);
                corner_ptr[chunkp1 * chunkp1 - 1] = *main;
                hmap_t *corner_right_pad = prev_row_prev_region->get_hmap_chunk_pad(ilod, corner_idx, ChunkPad::X_POS);
                corner_right_pad[chunk_size] = *(main + 1);
                hmap_t *corner_bottom_pad = prev_row_prev_region->get_hmap_chunk_pad(ilod, corner_idx, ChunkPad::Z_POS);
                corner_bottom_pad[chunk_size] = *(main + chunkp1);
                rsize >>= 1;
            }
        }
    }

    if (p_region.z != 0) {
        const size_t prev_row_idx = (p_pool_index + 2) % pool_size;
        Region *prev_row_region = p_regions_pool[prev_row_idx];
        size_t rsize = region_size >> 1;

        for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
            for (size_t ichunk = 0; ichunk < rsize; ++ichunk) {
                hmap_t *top_pad = get_hmap_chunk_pad(ilod, ichunk, ChunkPad::Z_NEG);
                size_t prev_row_chunk_idx = ichunk + rsize * (rsize - 1);
                hmap_t *prev_row_ptr = prev_row_region->get_hmap_chunk(ilod, prev_row_chunk_idx) + chunkp1 * (chunk_size - 1);
                memcpy(top_pad, prev_row_ptr, chunkp1);
                const hmap_t *main_ptr = get_hmap_chunk(ilod, ichunk);
                memcpy(prev_row_ptr + chunkp1, main_ptr, chunkp1);
                memcpy(prev_row_region->get_hmap_chunk_pad(ilod, prev_row_chunk_idx, ChunkPad::Z_POS), main_ptr + chunkp1, chunkp1);
            }

            rsize >>= 1;
        }

        if (p_region.x < p_regions.x - 1) {
            const size_t prev_row_next_idx = (p_pool_index + 3) % pool_size;
            Region *prev_row_next_region = p_regions_pool[prev_row_next_idx];
            rsize = region_size >> 1;

            for (size_t ilod = 1; ilod < specs.region_lods; ++ilod) {
                hmap_t *corner_left_pad = prev_row_next_region->get_hmap_chunk_pad(ilod, rsize * (rsize - 1), ChunkPad::X_NEG);
                corner_left_pad[chunk_size] = *(get_hmap_chunk(ilod, rsize - 1) + chunk_size - 1);
                get_hmap_chunk_pad(ilod, rsize - 1, ChunkPad::Z_NEG)[chunk_size] = *(prev_row_next_region->get_hmap_chunk(ilod, rsize * (rsize - 1)) + chunkp1 * (chunk_size - 1));
                rsize >>= 1;
            }
        }
    }
}

void Region::store_hmap() const {
    write_header();
    access->store_buffer(buffer, specs.get_buffer_size());
}

PackedInt32Array Region::get_hmap_chunk_values(size_t p_lod, const CellKey &p_chunk) const {
    // TODO: Make sure hmap is loaded.
    ERR_FAIL_INDEX_V_EDMSG(p_lod, specs.region_lods, PackedInt32Array(), "LOD out of range.");
    const size_t region_side = specs.region_size >> p_lod;
    ERR_FAIL_INDEX_V_EDMSG(p_chunk.x, region_side, PackedInt32Array(), vformat("Chunk x index (%d) out of range (%d).", p_chunk.x, region_side));
    ERR_FAIL_INDEX_V_EDMSG(p_chunk.z, region_side, PackedInt32Array(), vformat("Chunk z index (%d) out of range (%d).", p_chunk.z, region_side));
    const size_t chunk_idx = p_chunk.x + p_chunk.z * region_side;
    const hmap_t *hmap_ptr = get_hmap_chunk(p_lod, chunk_idx);
    PackedInt32Array values;
    size_t size = specs.chunk_size * specs.chunk_size;
    values.resize(size);
    int *values_ptr = values.ptrw();

    for (int iz = 0; iz < specs.chunk_size; ++iz) {
        for (int ix = 0; ix < specs.chunk_size; ++ix) {
            *values_ptr = (int)*hmap_ptr;
            values_ptr++;
            hmap_ptr++;
        }

        hmap_ptr++;
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

Region::hmap_t *Region::get_hmap_chunk(size_t p_lod, size_t p_chunk_idx) const {
    size_t chunk_size = specs.hmap_lod_chunk_sizes[p_lod];
    return hmap_buffer + specs.hmap_lod_offsets[p_lod] + chunk_size * p_chunk_idx;
}

Region::hmap_t *Region::get_hmap_chunk_pad(size_t p_lod, size_t p_chunk_idx, ChunkPad p_pad) const {
    size_t chunk_size = specs.hmap_lod_chunk_sizes[p_lod];
    size_t chunkp1 = specs.chunk_size + 1;
    return hmap_buffer + specs.hmap_lod_offsets[p_lod] + chunk_size * p_chunk_idx + chunkp1 * chunkp1 + static_cast<size_t>(p_pad) * chunkp1;
}

Region::Region(const Specs &p_specs, const Ref<FileAccess> &p_access)
    : specs(p_specs), access(p_access)
{ }

Region::~Region() {
    if (buffer) {
        memfree(buffer);
        buffer = nullptr;
        minmax_buffer = nullptr;
        hmap_buffer = nullptr;
    }
}