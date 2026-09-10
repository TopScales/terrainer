/**
 * map_storage.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#include "map_storage.h"

#include "core/object/class_db.h"
#include "../utils/math.h"

using namespace Terrainer;

const StringName MapStorage::path_changed = "path_changed";
const String MapStorage::REGION_FILE_BASE_NAME("region_");
const String MapStorage::REGION_FILE_EXTENSION("map");
const String MapStorage::REGION_FILE_FORMAT(REGION_FILE_BASE_NAME + "%d_%d." + REGION_FILE_EXTENSION);

void MapStorage::store_heightmap_data(const PackedByteArray &p_data, const Vector2i &p_size) {
    ERR_FAIL_COND_EDMSG(directory_path.is_empty(), "Empty directory path.");
    ERR_FAIL_COND_EDMSG(!DirAccess::exists(directory_path), "Storage directory does not exist.");
    ERR_FAIL_COND_EDMSG(data_locked, "Failed to store data: data is locked.");
    ERR_FAIL_COND_EDMSG(p_data.size() != p_size.x * p_size.y, "Incorrect data buffer size.");
    clear();

    if (specs.dirty) {
        specs.config();
    }

    const int32_t region_cells = specs.region_size * specs.chunk_size;
    const CellKey data_regions = CellKey((p_size.x + 1) / region_cells, (p_size.y + 1) / region_cells);
    const size_t pool_size = data_regions.x + 2;
    Vector<Region *> regions_pool;
    size_t pool_index = 0;
    regions_pool.resize(pool_size);

    for (uint16_t reg_iz = 0; reg_iz < data_regions.z; ++reg_iz) {
        for (uint16_t reg_ix = 0; reg_ix < data_regions.x; ++reg_ix) {
            const CellKey region_key = {reg_ix, reg_iz};
            String file_path = vformat(REGION_FILE_FORMAT, reg_ix, reg_iz);
            Error error;
            Ref<FileAccess> file = FileAccess::open(directory_path.path_join(file_path), FileAccess::WRITE_READ, &error);
            Region *region = memnew(Region(specs, file));
            region->load_hmap_region(region_key, data_regions, p_data, p_size);
            regions_pool.write[pool_index] = region;
            region->fill_hmap_region_pad(region_key, data_regions, regions_pool, pool_index);
            regions[region_key] = region;
            pool_index = (pool_index + 1) % pool_size;
        }
    }

    for (KeyValue<CellKey, Region*> &kv : regions) {
        Region *region = kv.value;
        region->store_hmap();
    }
}

Error MapStorage::load_headers() {
    if (directory_path.is_empty()) {
        return ERR_FILE_BAD_PATH;
    } else if (!DirAccess::exists(directory_path)) {
        return ERR_FILE_NOT_FOUND;
    }

    if (specs.dirty) {
        specs.config();
    }

    Error error;
    Ref<DirAccess> dir = DirAccess::open(directory_path, &error);
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Error while opening MapStorage directory.");
    error = dir->list_dir_begin();
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Can't iterate over files in MapStorage directory.");
    const int mode = data_locked ? FileAccess::READ : FileAccess::READ_WRITE;

    while (true) {
        String file_name = dir->get_next();

        if (file_name.is_empty()) {
            break;
        }

        if (!dir->current_is_dir() && file_name.begins_with(REGION_FILE_BASE_NAME) && file_name.get_extension() == REGION_FILE_EXTENSION) {
            PackedStringArray parts = file_name.get_basename().split("_", false);

            if (parts.size() == 3 && parts[1].is_valid_int() && parts[2].is_valid_int()) {
                int x = parts[1].to_int();
                int z = parts[2].to_int();
                String file_path = directory_path.path_join(file_name);
                Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ, &error);
                ERR_CONTINUE_EDMSG(error != OK, vformat("Can`t open stream region file %s.", file_path));
                Region *region = memnew(Region(specs, file));

                if (region->load()) {
                    regions[CellKey(x, z)] = region;
                } else {
                    memdelete(region);
                }
            }
        }
    }

    dir->list_dir_end();
    return OK;
}

void MapStorage::clear() {
    for (KeyValue<CellKey, Region*> &kv : regions) {
        Region *region = kv.value;
        memdelete(region);
    }

    regions.clear();

//     if (minmax_buffer) {
//         memdelete(minmax_buffer);
//     }

//     if (hmap_buffer) {
//         memdelete(hmap_buffer);
//     }

//     minmax_trackers.clear();

//     for (int i = 0; i < textures_trackers.size(); ++i) {
//         for (KeyValue<NodeKey, Tracker> &kv : textures_trackers.get(i)) {
//             Tracker &tracker = kv.value;
//             TextureData *td = (TextureData *)tracker.pointer;
//             memdelete(td);
//         }
//     }

//     textures_trackers.clear();
}

bool MapStorage::has_region(const Vector2i &p_region) const {
    return regions.has(p_region);
}

int MapStorage::get_num_regions() const {
    return regions.size();
}

PackedInt32Array MapStorage::get_chunk_hmap(const Vector2i &p_region, int p_lod, const Vector2i &p_chunk) const {
    const Region *const *region_ptr = regions.getptr(p_region);
    ERR_FAIL_NULL_V_EDMSG(region_ptr, PackedInt32Array(), "Region not present in map or not loaded.");
    const Region *region = *region_ptr;
    return region->get_hmap_chunk_values(p_lod, p_chunk);
}

// PackedByteArray MapStorage::get_region_hmap_buffer(const Vector2i &p_region) {
//     Region **region_ptr = regions.getptr(p_region);
//     ERR_FAIL_NULL_V_EDMSG(region_ptr, PackedByteArray(), "Region not present in map.");
//     Region *region = *region_ptr;

//     if (region->is_hmap_loaded()) {
//         return region->hmap->to_byte_array();
//     } else {
//         Ref<FileAccess> file = region->query_access;
//         file->seek(region->header->hmap_offset);
//         region->hmap = memnew(LODBuffer<hmap_t>(hmap_specs));
//         LODBuffer<hmap_t> &hmap = *region->hmap;
//         file->get_buffer(hmap.bytesw(), hmap.size());
//         region->set_hmap_loaded(true);
//         return hmap.to_byte_array();
//     }
// }

// PackedInt32Array MapStorage::get_chunk_hmap(const Vector2i &p_region, const Vector2i &p_chunk) {
//     Region **region_ptr = regions.getptr(p_region);
//     ERR_FAIL_NULL_V_EDMSG(region_ptr, PackedInt32Array(), "Region not present in map.");
//     Region *region = *region_ptr;

//     if (!region->is_hmap_loaded()) {
//         Ref<FileAccess> file = region->query_access;
//         file->seek(region->header->hmap_offset);
//         region->hmap = memnew(LODBuffer<hmap_t>(hmap_specs));
//         LODBuffer<hmap_t> &hmap = *region->hmap;
//         file->get_buffer(hmap.bytesw(), hmap.size());
//         region->set_hmap_loaded(true);
//     }

//     size_t chunk_idx = p_chunk.x + p_chunk.y * region_size;
//     const hmap_t *hmap_ptr = region->hmap->ptr(0, chunk_idx);
//     PackedInt32Array res;
//     res.resize(chunk_size * chunk_size);
//     int *res_ptr = res.ptrw();

//     for (int iz = 0; iz < chunk_size; ++iz) {
//         for (int ix = 0; ix < chunk_size; ++ix) {
//             *res_ptr = (int)*hmap_ptr;
//             res_ptr++;
//             hmap_ptr++;
//         }

//         hmap_ptr++;
//     }

//     return res;
// }

// bool MapStorage::is_sector_loaded(CellKey p_sector) const {
    // _cache_minmax(p_sector);
    // cached_minmax_tracker->frame = current_frame;
    // return cached_minmax_tracker->is_loaded();
// }

// void MapStorage::load_minmax(CellKey p_sector, bool p_in_frustum) {
//     _cache_minmax(p_sector);

//     if (cached_minmax_tracker->exists()) {
//         cached_minmax_tracker->in_frustum = p_in_frustum;
//     } else {
//         Tracker *tracker = nullptr;

//         if (sector_size < region_size) {
//             uint16_t region_sectors = region_size / sector_size;
//             const CellKey region_key = CellKey(p_sector.x / region_sectors, p_sector.z / region_sectors);

//             for (uint16_t izs = 0; izs < region_sectors; ++izs) {
//                 const int z_sector = izs + region_key.z * region_sectors;

//                 for (uint16_t ixs = 0; ixs < region_sectors; ++ixs) {
//                     const uint16_t x_sector = ixs + region_key.x * region_sectors;
//                     const CellKey sector_key = CellKey(x_sector, z_sector);
//                     auto it = minmax_trackers.insert(sector_key, {current_frame, Tracker::Status::LOADING, p_in_frustum});
//                     tracker = &it->value;
//                 }
//             }
//         } else {
//             auto it = minmax_trackers.insert(p_sector, {current_frame, Tracker::Status::LOADING, p_in_frustum});
//             tracker = &it->value;
//         }

//         _add_request(NodeKey(p_sector, CellKey()), tracker, DATA_TYPE_MINMAX, 0);
//     }
// }

void MapStorage::get_minmax(const NodeKey &p_key, int p_lod, hmap_t &r_min, hmap_t &r_max) {
    Sector **sector_ptr = sectors.getptr(p_key.sector);
    Sector *sector = nullptr;

    if (sector_ptr) {
        sector = *sector_ptr;
    } else {
        sector = memnew(Sector(p_key.sector, regions, specs));
        sectors[p_key.sector] = sector;
    }

    sector->get_minmax(p_key.cell, p_lod, r_min, r_max);

//     _cache_minmax(p_key.sector);

//     if (cached_minmax_tracker->is_loaded()) {
//         hmap_t *minmax = (hmap_t *)cached_minmax_tracker->pointer;
//         const size_t lod_offset = minmax_lod_offsets[p_lod];
//         const size_t block_size = sector_size >> p_lod;
//         const size_t cell_offset = 2 * (p_key.cell.x + block_size * p_key.cell.z);
//         const size_t offset = lod_offset + cell_offset;
//         r_min = minmax[offset];
//         r_max = minmax[offset + 1];
//     } else {
//         r_min = 0;
//         r_max = HMAP_MAX;
//     }

//     r_has_data = cached_minmax_tracker->is_loaded();
}

void MapStorage::allocate_buffers(int p_sector_chunks, int p_num_nodes, int p_lods, const Vector3 &p_map_scale, real_t p_far_view) {
    stop_io();
    specs.set_sector_info(p_sector_chunks, p_lods);
    // map_scale = p_map_scale;
    // const int sector_cells = sector_size * chunk_size;
    // const real_t sector_world_size_x = sector_cells * map_scale.x;
    // const real_t sector_world_size_z = sector_cells * map_scale.z;
    // size_t blocks_x = Math::ceil(2.0 * p_far_view / sector_world_size_x) + 1;
    // size_t blocks_z = Math::ceil(2.0 * p_far_view / sector_world_size_z) + 1;
    // camera_far = p_far_view;

    // if (sector_size < region_size) {
    //     blocks_x = region_size * (size_t)Math::ceil(real_t(sector_size * blocks_x) / real_t(region_size)) / sector_size + 1;
    //     blocks_z = region_size * (size_t)Math::ceil(real_t(sector_size * blocks_z) / real_t(region_size)) / sector_size + 1;
    // }

    // const size_t minmax_block_count = blocks_x * blocks_z * BUFFER_EXTRA_ALLOCATION_FACTOR;
//     minmax_lod_offsets.resize(lods);
//     hmap_lod_offset.resize(lods);
//     size_t minmax_block_size = 0;
//     size_t minmax_lod_block_size = 2 * sector_size * sector_size; // Shouldn't be using region size???
//     cancelled_frame = current_frame++;
//     size_t hmap_block_size = chunk_size;
//     size_t hmap_offset = 0;

//     for (int ilod = 0; ilod < lods; ++ilod) {
//         minmax_lod_offsets.set(ilod, minmax_block_size);
//         hmap_lod_offset.set(ilod, hmap_offset);
//         const size_t hmap_lod_block = (hmap_block_size + 1) * (hmap_block_size + 1) + 4 * (hmap_block_size + 1);
//         hmap_offset += hmap_lod_block;
//         hmap_block_size >>= 1;
//         minmax_block_size += minmax_lod_block_size;
//         minmax_lod_block_size >>= 2;
//     }

//     if (minmax_buffer) {
//         if (minmax_buffer->get_block_size() != minmax_block_size && !minmax_read.is_empty()) {
//             minmax_read.clear();
//         }

//         if (minmax_buffer->get_block_size() != minmax_block_size || minmax_buffer->get_block_count() != minmax_block_count) {
//             memdelete(minmax_buffer);
//             minmax_buffer = nullptr;
//             minmax_trackers.clear();
//         }
//     }

//     if (!minmax_buffer) {
//         minmax_buffer = memnew(BufferPool<hmap_t>(minmax_block_size, minmax_block_count));
//     }


//     if (minmax_read.is_empty() && sector_size != region_size) {
//         const int read_size = lod_expand(region_size, MIN(lods, saved_lods));
//         minmax_read.resize(read_size);
//     }

//     textures_trackers.resize(lods);
//     _allocate_textures(p_num_nodes);
//     const size_t hmap_count = p_num_nodes * hmap_buffer_size_factor;
//     const size_t hmap_size = (chunk_size + 1) * (chunk_size + 1);

//     if (hmap_buffer && hmap_buffer->get_block_count() != hmap_count || hmap_buffer->get_block_size() != hmap_size) {
//         memdelete(hmap_buffer);
//         hmap_buffer = nullptr;
//         memdelete(hmap_load);

//         for (int i = 0; i < textures_trackers.size(); ++i) {
//             for (KeyValue<NodeKey, Tracker> &kv : textures_trackers.get(i)) {
//                 Tracker &tracker = kv.value;
//                 TextureData *td = (TextureData *)tracker.pointer;
//                 memdelete(td);
//             }
//         }

//         textures_trackers.clear();
//     }

//     if (!hmap_buffer) {
//         hmap_buffer = memnew(VectorBufferPool<hmap_t>(hmap_size, hmap_count));
//         hmap_load = memnew(AlignedBuffer<hmap_t>(hmap_size + 4 * (chunk_size + 1)));
//     }
}

// uint16_t MapStorage::get_node_texture_layer(const NodeKey &p_key, int p_lod) {
//     ERR_FAIL_INDEX_V_EDMSG(p_lod, lods, 0, "Incorrect LOD level.");
//     HashMap<NodeKey, Tracker> &map = textures_trackers.write[p_lod];
//     Tracker *tracker = map.getptr(p_key);

//     if (tracker) {
//         tracker->frame = current_frame;
//         tracker->in_frustum = true;
//         TextureData *td = (TextureData *)tracker->pointer;
//         return td->layer;
//     } else {
//         const auto it = map.insert(p_key, {current_frame, Tracker::Status::LOADING, true});
//         tracker = &it->value;
//         TextureData *td = memnew(TextureData);
//         tracker->pointer = td;
//         _add_request(p_key, tracker, DATA_TYPE_HEIGHT | DATA_TYPE_SPLAT, p_lod);
//         return INVALID_TEXTURE_LAYER;
//     }
// }

void MapStorage::update_viewer(const Vector3 &p_viewer_pos, const Vector3 &p_viewer_vel, const Vector3 &p_viewer_forward) {
    viewer_pos = p_viewer_pos;
    viewer_vel = p_viewer_vel;
    viewer_forward = p_viewer_forward;
    predicted_viewer_pos = viewer_pos + viewer_vel * PRIORITY_PREDICTION_DELTA_TIME;
}

void MapStorage::stop_io() {
    // if (io_thread.is_started()) {
    //     io_running.clear();
    //     cancelled_frame = current_frame;
    //     io_pending.clear();

    //     while (io_queue->front()) {
    //         io_queue->pop();
    //     }

    //     io_thread.wait_to_finish();

    //     while (io_result->front()) {
    //         io_result->pop();
    //     }
    // }
}

void MapStorage::process() {
    // _submit_requests();
    // _process_results();
    // _clean_minmax();
    // current_frame++;
}

// int MapStorage::get_buffer_stat(BufferType p_buffer, BufferStat p_stat) const {
//     switch (p_buffer) {
//     case BUFFER_MINMAX:
//         switch (p_stat) {
//             case STAT_ALLOCATED_COUNT:
//                 return minmax_buffer->get_allocated_count();
//             case STAT_AVAILABLE_BLOCKS:
//                 return minmax_buffer->get_available_blocks();
//             case STAT_AVAILABLE_BYTES:
//                 return minmax_buffer->get_available_bytes();
//             case STAT_BLOCK_COUNT:
//                 return minmax_buffer->get_block_count();
//             case STAT_BLOCK_SIZE:
//                 return minmax_buffer->get_block_size();
//             case STAT_FREE_COUNT:
//                 return minmax_buffer->get_free_count();
//             case STAT_PEAK_ALLOCATED:
//                 return minmax_buffer->get_peak_allocated();
//             case STAT_TOTAL_ALLOCATIONS:
//                 return minmax_buffer->get_total_allocations();
//             case STAT_TOTAL_DEALLOCATIONS:
//                 return minmax_buffer->get_total_deallocations();
//             case STAT_UTILIZATION:
//                 return (int)Math::round(100.0 * minmax_buffer->get_utilization());
//             default:
//                 return -1;
//         }
//     case BUFFER_HMAP:
//         switch (p_stat) {
//             case STAT_ALLOCATED_COUNT:
//                 return hmap_buffer->get_allocated_count();
//             case STAT_AVAILABLE_BLOCKS:
//                 return hmap_buffer->get_available_blocks();
//             case STAT_AVAILABLE_BYTES:
//                 return hmap_buffer->get_available_bytes();
//             case STAT_BLOCK_COUNT:
//                 return hmap_buffer->get_block_count();
//             case STAT_BLOCK_SIZE:
//                 return hmap_buffer->get_block_size();
//             case STAT_FREE_COUNT:
//                 return hmap_buffer->get_free_count();
//             case STAT_PEAK_ALLOCATED:
//                 return hmap_buffer->get_peak_allocated();
//             case STAT_TOTAL_ALLOCATIONS:
//                 return hmap_buffer->get_total_allocations();
//             case STAT_TOTAL_DEALLOCATIONS:
//                 return hmap_buffer->get_total_deallocations();
//             case STAT_UTILIZATION:
//                 return (int)Math::round(100.0 * hmap_buffer->get_utilization());
//             default:
//                 return -1;
//             }
//     default:
//         return -1;
//     }
// }

bool MapStorage::is_directory_set() const {
    return directory_path.is_empty() ? false : DirAccess::exists(directory_path);
}

void MapStorage::set_directory_path(const String &p_path) {
    directory_path = p_path;
    clear();
    emit_signal(path_changed);
}

String MapStorage::get_directory_path() const {
    return directory_path;
}

void MapStorage::set_chunk_size(int p_size) {
    if (!size_locked) {
        ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain chunk size must be greater than zero.");
        ERR_FAIL_COND_EDMSG(p_size > MAX_CHUNK_SIZE, vformat("Terrain chunk size must be at most %d.", MAX_CHUNK_SIZE));
        const int size = MAX(round_po2(p_size, specs.chunk_size), 2);

        if (size != specs.chunk_size) {
            specs.chunk_size = size;
            specs.dirty = true;
            clear();
            emit_changed();
        }
    }
}

int MapStorage::get_chunk_size() const {
    return specs.chunk_size;
}

void MapStorage::set_region_size(int p_size) {
    if (!size_locked) {
        ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain region size must be greater than zero.");
        const int size = MAX(round_po2(p_size, specs.region_size), 2);

        if (size != specs.region_size) {
            specs.region_size = size;
            specs.dirty = true;
            clear();
            emit_changed();
        }
    }
}

int MapStorage::get_region_size() const {
    return specs.region_size;
}

void MapStorage::set_size_locked(bool p_locked) {
    size_locked = p_locked;
    notify_property_list_changed();
}

bool MapStorage::is_size_locked() const {
    return size_locked;
}

void MapStorage::set_data_locked(bool p_locked) {
    data_locked = p_locked;
}

bool MapStorage::is_data_locked() const {
    return data_locked;
}

void MapStorage::set_default_height(hmap_t p_height) {
    specs.default_height = MIN(p_height, HMAP_MAX - 1);
    specs.default_minmax = Region::MinMax(specs.default_height, specs.default_height + 1);
}

// bool MapStorage::_set(const StringName &p_name, const Variant &p_value) {
//     String prop_name = p_name;

//     if (prop_name == "chunk_size") {
//         set_chunk_size(p_value);
//         return true;
//     } else if (prop_name == "region_size") {
//         set_region_size(p_value);
//         return true;
//     } else if (prop_name == "size_locked") {
//         set_size_locked(p_value);
//     } else if (prop_name == "data_locked") {
//         set_data_locked(p_value);
//     }

//     return false;
// }

// int MapStorage::get_minmax_allocated_sectors() const {
//     return minmax_buffer ? minmax_buffer->get_block_count() / 2 : 0;
// }


void MapStorage::_validate_property(PropertyInfo &p_property) const {
	if (!Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (size_locked && (p_property.name == "chunk_size" || p_property.name == "region_size")) {
        p_property.usage = PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY;
	}
}

void MapStorage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_directory_path", "path"), &MapStorage::set_directory_path);
	ClassDB::bind_method(D_METHOD("get_directory_path"), &MapStorage::get_directory_path);
    ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &MapStorage::set_chunk_size);
    ClassDB::bind_method(D_METHOD("get_chunk_size"), &MapStorage::get_chunk_size);
    ClassDB::bind_method(D_METHOD("set_region_size", "size"), &MapStorage::set_region_size);
    ClassDB::bind_method(D_METHOD("get_region_size"), &MapStorage::get_region_size);
    ClassDB::bind_method(D_METHOD("set_size_locked", "locked"), &MapStorage::set_size_locked);
    ClassDB::bind_method(D_METHOD("is_size_locked"), &MapStorage::is_size_locked);
    ClassDB::bind_method(D_METHOD("set_data_locked", "locked"), &MapStorage::set_data_locked);
    ClassDB::bind_method(D_METHOD("is_data_locked"), &MapStorage::is_data_locked);
//     ClassDB::bind_method(D_METHOD("get_buffer_stat", "buffer", "stat"), &MapStorage::get_buffer_stat);

    ClassDB::bind_method(D_METHOD("store_heightmap_data", "data", "size"), &MapStorage::store_heightmap_data);
    ClassDB::bind_method(D_METHOD("has_region", "region"), &MapStorage::has_region);
    ClassDB::bind_method(D_METHOD("get_num_regions"), &MapStorage::get_num_regions);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "directory_path", PROPERTY_HINT_DIR), "set_directory_path", "get_directory_path");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size", PROPERTY_HINT_RANGE, "2,256,1"), "set_chunk_size", "get_chunk_size");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "region_size", PROPERTY_HINT_RANGE, "2,256,1"), "set_region_size", "get_region_size");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "size_locked", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_size_locked", "is_size_locked");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "data_locked", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_data_locked", "is_data_locked");

    ADD_SIGNAL(MethodInfo(path_changed));

    BIND_CONSTANT(MAX_CHUNK_SIZE);
    BIND_CONSTANT(MAX_LOD_LEVELS);

//     BIND_ENUM_CONSTANT(BUFFER_MINMAX);
//     BIND_ENUM_CONSTANT(BUFFER_HMAP);

//     BIND_ENUM_CONSTANT(STAT_ALLOCATED_COUNT);
//     BIND_ENUM_CONSTANT(STAT_FREE_COUNT);
//     BIND_ENUM_CONSTANT(STAT_PEAK_ALLOCATED);
//     BIND_ENUM_CONSTANT(STAT_TOTAL_ALLOCATIONS);
//     BIND_ENUM_CONSTANT(STAT_TOTAL_DEALLOCATIONS);
//     BIND_ENUM_CONSTANT(STAT_UTILIZATION);
//     BIND_ENUM_CONSTANT(STAT_AVAILABLE_BLOCKS);
//     BIND_ENUM_CONSTANT(STAT_AVAILABLE_BYTES);
//     BIND_ENUM_CONSTANT(STAT_BLOCK_SIZE);
//     BIND_ENUM_CONSTANT(STAT_BLOCK_COUNT);
}

// void MapStorage::_process_requests(void *p_storage) {
//     MapStorage *storage = static_cast<MapStorage *>(p_storage);

//     while (storage->io_running.is_set()) {
//         if (storage->io_queue->front()) {
//             IORequest *request = storage->io_queue->front();

//             if (request->data_type == DATA_TYPE_MINMAX) {
//                 storage->_load_sector_minmax(request->key, *request);
//             }

//             storage->io_queue->pop();
//         }
//     }
// }

// void MapStorage::_add_request(const NodeKey &p_key, Tracker *p_tracker, uint16_t p_data_type, uint16_t p_lod) {
//     io_pending.push_back({p_key, p_tracker, current_request++, p_data_type, p_lod});
// }

// void MapStorage::_submit_requests() {
//     if (io_queue->size() == MAX_QUEUE_SIZE || io_pending.is_empty()) {
//         if (!io_thread.is_started()) {
//             io_running.set();
//             io_thread.start(_process_requests, this);
//         }

//         return;
//     }

//     const int sector_cells = sector_size * chunk_size;

//     for (IORequest &request : io_pending) {
//         if (request.data_type == DATA_TYPE_MINMAX) {
//             const Vector3 p = request.key.sector_position(sector_cells * map_scale.x, sector_cells * map_scale.z);
//             request.priority = PRIORITY_MINMAX * _calc_request_priority(p, request.tracker->in_frustum);
//         } else if (request.data_type & (DATA_TYPE_HEIGHT | DATA_TYPE_SPLAT)) {
//             const Vector3 p = request.key.position(sector_cells, request.lod_level, lods, map_scale.x, map_scale.z);
//             request.priority = _calc_request_priority(p, request.tracker->in_frustum && (current_frame == request.tracker->frame)) + MAX_LOD_LEVELS - request.lod_level;
//         }
//     }

//     io_pending.sort_custom<RequestCompare>();
//     int submitted = 0;
//     int ipending = io_pending.size() - 1;

//     while (ipending >= 0 && submitted < MAX_QUEUE_SIZE) {
//         if (io_queue->try_push(io_pending[ipending])) {
//             ipending--;
//             submitted++;
//         } else {
//             break;
//         }
//     }

//     io_pending.resize(ipending + 1);

//     if (!io_thread.is_started()) {
//         io_running.set();
//         io_thread.start(_process_requests, this);
//     }
// }

// void MapStorage::_process_results() {
//     int processed = 0;

//     while (io_result->front() && processed < MAX_PROCESSED_RESULTS) {
//         IOResult *result = io_result->front();

//         if (result->data_type == DATA_TYPE_MINMAX) {
//             Tracker &tracker = *minmax_trackers.getptr(result->key.sector);
//             tracker.pointer = result->pointer;
//             tracker.status = Tracker::Status::LOADED;
//         }

//         io_result->pop();
//         processed++;
//     }
// }

// void MapStorage::_load_region_minmax(CellKey p_region_key, hmap_t *p_buffer, size_t p_size) {
//     Region **region_ptr = regions.getptr(p_region_key);
//     Region *region = nullptr;

//     if (region_ptr) {
//         region = *region_ptr;
//     } else {
//         region = _create_region(p_region_key);
//     }

//     if (region->header->has_minmax()) {
//         region->data_access->seek(MINMAX_OFFSET);
//         size_t nbytes = p_size * sizeof(hmap_t);
//         int64_t len = region->data_access->get_buffer(reinterpret_cast<uint8_t*>(p_buffer), nbytes);
//         ERR_FAIL_COND_EDMSG(len != nbytes, "Returned buffer of different size than expected.");
//     } else {
//         hmap_t hmax = default_height + 1;

//         for (int i = 0; i < p_size; i += 2) {
//             p_buffer[i] = default_height;
//             p_buffer[i + 1] = hmax;
//         }
//     }
// }

// void MapStorage::_load_sector_minmax(const NodeKey &p_key, const IORequest &p_request) {
//     if (sector_size < region_size) {
//         const uint16_t region_sectors = region_size / sector_size;
//         Vector<hmap_t *> buffers;
//         buffers.resize(region_sectors * region_sectors);
//         size_t allocated_sectors = minmax_buffer->allocate_batch(buffers.ptrw(), region_sectors * region_sectors);

//         if (allocated_sectors != region_sectors * region_sectors) {
//             minmax_buffer->free_batch(buffers.ptrw(), allocated_sectors);
//             IOResult res = IOResult(p_key, p_request.request_id, DATA_TYPE_MINMAX, 0);
//             res.status = IOResult::Status::OUT_OF_MEMORY;
//             io_result->push(res);
//             return;
//         }

//         const CellKey region_key = CellKey(p_key.sector.x / region_sectors, p_key.sector.z / region_sectors);
//         uint16_t *src = minmax_read.ptrw();
//         _load_region_minmax(region_key, src, minmax_read.size());

//         for (int izs = 0; izs < region_sectors; ++izs) {
//             const int z_sector = izs + region_key.z * region_sectors;

//             for (int ixs = 0; ixs < region_sectors; ++ixs) {
//                 const int x_sector = ixs + region_key.x * region_sectors;
//                 const CellKey sector_key = CellKey(x_sector, z_sector);
//                 size_t write_size = 2 * sector_size * sizeof(hmap_t);
//                 size_t rows = sector_size;
//                 hmap_t *sector_buffer = buffers[ixs + izs * region_sectors];
//                 IOResult res = IOResult({sector_key, CellKey()}, p_request.request_id, DATA_TYPE_MINMAX, 0);
//                 res.pointer = sector_buffer;
//                 ptrdiff_t buffer_offset = 0;
//                 ptrdiff_t src_lod_offset = 0;
//                 ptrdiff_t src_block_size = 2 * region_size * region_size;

//                 for (int ilod = 0; ilod < lods; ++ilod) {
//                     const ptrdiff_t src_offset = src_lod_offset + 2 * ixs * rows + 2 * izs * rows * rows * region_sectors;

//                     for (int iz = 0; iz < rows; ++iz) {
//                         const ptrdiff_t src_index = src_offset + 2 * iz * rows * region_sectors;
//                         memcpy(sector_buffer + buffer_offset, src + src_index, write_size);
//                         buffer_offset += 2 * rows;
//                     }

//                     src_lod_offset += src_block_size;
//                     src_block_size >>= 2;
//                     write_size >>= 1;
//                     rows >>= 1;
//                 }

//                 res.status = IOResult::Status::SUCCESS;
//                 io_result->push(res);
//             }
//         }
//     } else {
//         hmap_t *sector_buffer = minmax_buffer->allocate();

//         if (!sector_buffer) {
//             IOResult res = IOResult(p_key, p_request.request_id, DATA_TYPE_MINMAX, 0);
//             res.status = IOResult::Status::OUT_OF_MEMORY;
//             io_result->push(res);
//             return;
//         }

//         IOResult res = IOResult(p_key, p_request.request_id, DATA_TYPE_MINMAX, 0);
//         res.pointer = sector_buffer;

//         if (sector_size == region_size) {
//             _load_region_minmax(p_key.sector, sector_buffer, minmax_buffer->get_block_size());
//         } else { // sector_size > region_size
//             int sector_regions = sector_size / region_size;
//             int num_lods = MIN(saved_lods, lods);

//             for (int izr = 0; izr < sector_regions; ++izr) {
//                 const int z_region = izr + p_key.sector.z * sector_regions;

//                 for (int ixr = 0; ixr < sector_regions; ++ixr) {
//                     const int x_region = ixr + p_key.sector.x * sector_regions;
//                     const CellKey region_key = CellKey(x_region, z_region);
//                     uint16_t *data = minmax_read.ptrw();
//                     _load_region_minmax(region_key, data, minmax_read.size());
//                     int read_size = 2 * region_size;

//                     for (int ilod = 0; ilod < num_lods; ++ilod) {
//                         const int lod_offset = minmax_lod_offsets[ilod];
//                         const int region_offset = read_size * ixr + izr * read_size * read_size * sector_regions;

//                         for (int iz = 0; iz < read_size; ++iz) {
//                             const ptrdiff_t buffer_index = lod_offset + region_offset + iz * read_size * sector_regions;
//                             memcpy(sector_buffer + buffer_index, data, read_size);
//                             data += read_size;
//                         }

//                         read_size >>= 1;
//                     }
//                 }
//             }

//             {
//                 // Fill in remaining LODs.
//                 int size = sector_size >> num_lods;

//                 for (int ilod = num_lods; ilod < lods; ++ilod) {
//                     const int src_lod_offset = minmax_lod_offsets[ilod - 1];
//                     const int dst_lod_offset = minmax_lod_offsets[ilod];

//                     for (int iz = 0; iz < size; ++iz) {
//                         for (int ix = 0; ix < size; ++ix) {
//                             const int src_index = src_lod_offset + 4 * (ix + 2 * iz * size);
//                             const int dst_index = dst_lod_offset + 2 * (ix + iz * size);
//                             const hmap_t src_min1 = sector_buffer[src_index];
//                             const hmap_t src_max1 = sector_buffer[src_index + 1];
//                             const hmap_t src_min2 = sector_buffer[src_index + 2];
//                             const hmap_t src_max2 = sector_buffer[src_index + 3];
//                             const hmap_t src_min3 = sector_buffer[src_index + 4 * size];
//                             const hmap_t src_max3 = sector_buffer[src_index + 4 * size + 1];
//                             const hmap_t src_min4 = sector_buffer[src_index + 4 * size + 2];
//                             const hmap_t src_max4 = sector_buffer[src_index + 4 * size + 3];
//                             sector_buffer[dst_index] = MIN(src_min1, MIN(src_min2, MIN(src_min3, src_min4)));
//                             sector_buffer[dst_index + 1] = MAX(src_max1, MAX(src_max2, MAX(src_max3, src_max4)));
//                         }
//                     }

//                     size >>= 1;
//                 }
//             }
//         }

//         res.status = IOResult::Status::SUCCESS;
//         io_result->push(res);
//     }
// }

// MapStorage::Region *MapStorage::_create_region(CellKey p_region_key) {
//     Region *region = memnew(Region);
//     Subheader *header = memnew(Subheader);
//     memset(header, 0, SUBHEADER_SIZE);
//     header->version = FORMAT_VERSION;
//     region->header = header;
//     regions[p_region_key] = region;
//     return region;
// }

// float MapStorage::_calc_request_priority(const Vector3 &p_chunk_pos, bool p_in_frustum) {
//     float distance = viewer_pos.distance_to(p_chunk_pos);
//     float predicted_distance = predicted_viewer_pos.distance_to(p_chunk_pos);
//     float effective_dist = MIN(distance, predicted_distance);
//     float priority = PRIORITY_DISTANCE_FACTOR * PRIORITY_DISTANCE_HALF_DECAY / (effective_dist + PRIORITY_DISTANCE_HALF_DECAY);

//     if (p_in_frustum) {
//         priority *= PRIORITY_IN_FRUSTUM;
//     }

//     Vector3 to_chunk = (p_chunk_pos - viewer_pos).normalized();
//     float dot = to_chunk.dot(viewer_forward);

//     if (dot < 0.0) {
//         // Behind camera, heavily reduce priority.
//         priority *= 0.1f;
//     } else {
//         // Ahead, bonus based on alignment.
//         priority *= (1.0f + dot * 0.5f);
//     }

//     return priority;
// }

// MapStorage::NodeKey MapStorage::_sector_to_region(const NodeKey &p_key, int p_lod) const {
//     if (sector_size == region_size) {
//         return p_key;
//     } else {
//         const uint32_t s = sector_size >> p_lod;
//         const uint32_t r = region_size >> p_lod;
//         uint32_t x = p_key.sector.x * s + p_key.cell.x;
//         uint32_t z = p_key.sector.z * s + p_key.cell.z;
//         CellKey region = CellKey(x / r, z / r);
//         CellKey cell = CellKey(x - r * region.x, z - r * region.z);
//         return NodeKey(region, cell);
//     }
// }

// void MapStorage::_clean_minmax() {
//     if (minmax_buffer && minmax_buffer->get_utilization() > CLEANUP_BUFFER_UTILIZATION) {
//         const int sector_cells = sector_size * chunk_size;
//         const real_t sector_world_size_x = sector_cells * map_scale.x;
//         const real_t sector_world_size_z = sector_cells * map_scale.z;
//         const Vector3 offset = Vector3(sector_world_size_x * 0.5, 0.0, sector_world_size_z * 0.5);
//         const real_t r2 = camera_far * camera_far;

//         for (KeyValue<CellKey, Tracker> &kv : minmax_trackers) {
//             Tracker &tracker = kv.value;

//             if (tracker.is_loaded()) {
//                 if (tracker.frame <= cancelled_frame) {
//                     minmax_buffer->free((hmap_t *)tracker.pointer);
//                 } else {
//                     const Vector3 p = kv.key.position(sector_world_size_x, sector_world_size_z) + offset;
//                     const Vector3 diff = p - viewer_pos;

//                     if (diff.x * diff.x + diff.z * diff.z > r2) {
//                         minmax_buffer->free((hmap_t *)tracker.pointer);
//                     }
//                 }
//             }
//         }

//         if (minmax_buffer->get_utilization() > CLEANUP_BUFFER_UTILIZATION) {
//             ERR_PRINT_ED("Failed to free MinMax buffers.");
//         }
//     }
// }

// void MapStorage::_cache_minmax(CellKey p_sector) const {
//     if (p_sector != cached_sector) {
//         cached_sector = p_sector;
//         const Tracker *tracker = minmax_trackers.getptr(cached_sector);

//         if (tracker) {
//             cached_minmax_tracker = tracker;
//         } else {
//             cached_minmax_tracker = &default_tracker;
//         }
//     }
// }

// void MapStorage::_allocate_textures(int p_layers) {
//     RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();

//     if (num_layers != 0) {
//         rd->free_rid(rd_heightmap_texture);
//     }

//     num_layers = p_layers * BUFFER_EXTRA_ALLOCATION_FACTOR;
//     RenderingDevice::TextureFormat height_format;
//     height_format.array_layers = num_layers;
//     height_format.format = RenderingDevice::DATA_FORMAT_R16_UINT;
//     height_format.width = chunk_size + 1;
//     height_format.height = chunk_size + 1;
//     height_format.mipmaps = 0;
//     height_format.texture_type = RenderingDevice::TEXTURE_TYPE_2D_ARRAY;
//     height_format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
//     RenderingDevice::TextureView tex_view;
//     rd_heightmap_texture = rd->texture_create(height_format, tex_view);
//     heightmap_texture->set_texture_rd_rid(rd_heightmap_texture);
//     used_layers = 0;
//     unused_texture_layers.clear();
// }

// int MapStorage::_next_layer() {
//     if (!unused_texture_layers.is_empty()) {
//         int new_size = unused_texture_layers.size() - 1;
//         int layer = unused_texture_layers[new_size];
//         unused_texture_layers.resize(new_size);
//         return layer;
//     } else {
//         return used_layers++;
//     }
// }

// void MapStorage::_load_hmap(const NodeKey &p_region_key, const NodeKey &p_sector_key, int p_lod, const IORequest &p_request) {
//     Region **region_ptr = regions.getptr(p_region_key.sector);
//     IOResult res = IOResult(p_sector_key, p_request.request_id, DATA_TYPE_HEIGHT, p_lod);
//     bool has_hmap = false;

//     if (region_ptr) {
//         Region *region = *region_ptr;

//         if (region->header->has_hmap()) {
//             has_hmap = true;
//             int64_t buffer_index = hmap_buffer->allocate();

//             if (buffer_index == VectorBufferPool<hmap_t>::INVALID_BUFFER) {
//                 res.status = IOResult::Status::OUT_OF_MEMORY;
//                 io_result->push(res);
//                 return;
//             }

//             hmap_t *buffer = hmap_buffer->ptrw(buffer_index);
//             // size_t hmap_index =
//         }
//     }



//         // hmap_t *sector_buffer = minmax_buffer->allocate();

//         // if (!sector_buffer) {
//         //     IOResult res = IOResult(p_key, p_request.request_id, DATA_TYPE_MINMAX, 0);
//         //     res.status = IOResult::Status::OUT_OF_MEMORY;
//         //     io_result->push(res);
//         //     return;
//         // }

//         // ERR_FAIL_NULL_EDMSG(sector_buffer, "Error allocating buffer to read minmax data.");
//         // IOResult res = IOResult(p_key, p_request.request_id, DATA_TYPE_MINMAX, 0);
//         // res.pointer = sector_buffer;


//         // res.status = IOResult::Status::SUCCESS;
//         // io_result->push(res);

//     // if (region_ptr) {
//     //     region = *region_ptr;
//     // } else {
//     //     region = _create_region(p_region_key);
//     // }

//     // if (region->header->has_minmax()) {
//     //     region->data_access->seek(MINMAX_OFFSET);
//     //     size_t nbytes = p_size * sizeof(hmap_t);
//     //     int64_t len = region->data_access->get_buffer(reinterpret_cast<uint8_t*>(p_buffer), nbytes);
//     //     ERR_FAIL_COND_EDMSG(len != nbytes, "Returned buffer of different size than expected.");
//     // } else {
//     //     hmap_t hmax = default_height + 1;

//     //     for (int i = 0; i < p_size; i += 2) {
//     //         p_buffer[i] = default_height;
//     //         p_buffer[i + 1] = hmax;
//     //     }
//     // }
// }

// void MapStorage::_clean_hmap() {
//     if (hmap_buffer && hmap_buffer->get_utilization() > CLEANUP_BUFFER_UTILIZATION) {
        // const int sector_cells = sector_size * chunk_size;
        // const real_t sector_world_size_x = sector_cells * map_scale.x;
        // const real_t sector_world_size_z = sector_cells * map_scale.z;
        // const Vector3 offset = Vector3(sector_world_size_x * 0.5, 0.0, sector_world_size_z * 0.5);
        // const real_t r2 = camera_far * camera_far;

        // for (int i = 0; i < lods; ++i) {
        //     HashMap<NodeKey, Tracker> &trackers = textures_trackers.get(i);

        //     for (KeyValue<NodeKey, Tracker> &kv : trackers) {
        //         Tracker &tracker = kv.value;
        //         TextureData *td = (TextureData *)tracker.pointer;

        //         // if (tracker.frame <= cancelled_frame) {
        //         //     hmap_buffer->free((hmap_t *)tracker.pointer);
        //         // }
        //     }
        // }

        //     if (tracker.is_loaded()) {
        //         if (tracker.frame <= cancelled_frame) {
        //             minmax_buffer->free((hmap_t *)tracker.pointer);
        //         } else {
        //             const Vector3 p = kv.key.position(sector_world_size_x, sector_world_size_z) + offset;
        //             const Vector3 diff = p - viewer_pos;

        //             if (diff.x * diff.x + diff.z * diff.z > r2) {
        //                 minmax_buffer->free((hmap_t *)tracker.pointer);
        //             }
        //         }
        //     }

        // if (minmax_buffer->get_utilization() > CLEANUP_BUFFER_UTILIZATION) {
        //     ERR_PRINT_ED("Failed to free MinMax buffers.");
//         }
//     }
// }

MapStorage::MapStorage() {
    // io_queue = memnew(SPSCQueue<IORequest>(MAX_QUEUE_SIZE));
    // io_result = memnew(SPSCQueue<IOResult>(MAX_RES_QUEUE_SIZE));
}

MapStorage::~MapStorage() {
    // stop_io();
    clear();
    // memdelete(io_queue);
    // memdelete(io_result);

    // if (num_layers != 0) {
    //     RenderingServer::get_singleton()->get_rendering_device()->free_rid(rd_heightmap_texture);
    // }
}
