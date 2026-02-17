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

#include "../utils/math.h"

using namespace Terrainer;

const String MapStorage::REGION_FILE_BASE_NAME("region_");
const String MapStorage::REGION_FILE_EXTENSION("bin");
const String MapStorage::REGION_FILE_FORMAT(REGION_FILE_BASE_NAME + "%d_%d." + REGION_FILE_EXTENSION);
const StringName MapStorage::path_changed = "path_changed";

Error MapStorage::load_headers() {
    if (!DirAccess::exists(directory_path)) {
        return ERR_FILE_BAD_PATH;
    }

    Error error;
    Ref<DirAccess> dir = DirAccess::open(directory_path, &error);
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Error while opening MapStorage directory.");
    error = dir->list_dir_begin();
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Can't iterate over files in MapStorage directory.");
    String file_name = dir->get_next();
    int mode = data_locked ? FileAccess::READ : FileAccess::READ_WRITE;

    while (!file_name.is_empty()) {
        if (!dir->current_is_dir() && file_name.begins_with(REGION_FILE_BASE_NAME) && file_name.get_extension() == REGION_FILE_EXTENSION) {
            PackedStringArray parts = file_name.get_basename().split("_", false);

            if (parts.size() == 3 && parts[1].is_valid_int() && parts[2].is_valid_int()) {
                int x = parts[1].to_int();
                int z = parts[2].to_int();
                String file_path = directory_path.path_join(file_name);
                Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ, &error);
                ERR_CONTINUE_EDMSG(error != OK, vformat("Can`t open stream region file %s.", file_path));
                ERR_CONTINUE_EDMSG(!_is_format_correct(file), vformat("Region file %s has incorrect format.", file_path));
                FileHeaderBytes *fh = new FileHeaderBytes;
                file->get_buffer(fh->bytes, FILE_HEADER_SIZE);
                error = file->get_error();
                ERR_CONTINUE_EDMSG(error != OK, vformat("Error (%d) while reading region file %s.", error, file_path));
                ERR_CONTINUE_EDMSG(fh->value.chunk_size != chunk_size, vformat("Wrong chunk size in region file %s.", file_name));
                ERR_CONTINUE_EDMSG(fh->value.region_size != region_size, vformat("Wrong region size in region file %s.", file_name));
                ERR_CONTINUE_EDMSG(fh->value.lods() != saved_lods, vformat("Wrong number of saved lods in region file %s.", file_name));
                Region *region = memnew(Region);
                region->data_access = FileAccess::open(file_path, mode, &error);
                ERR_CONTINUE_EDMSG(error != OK, vformat("Can`t open stream region data file %s.", file_path));
                region->data_access->big_endian = file->big_endian;
                region->header = memnew(Header);
                memcpy(region->header, &fh->value.header, HEADER_SIZE);
                delete fh;
                region->query_access = file;
                regions[Vector2i(x, z)] = region;
            }
        }

        file_name = dir->get_next();
    }

    dir->list_dir_end();
    return OK;
}

MapStorage::hmap_t* MapStorage::get_sector_minmax_ptr(const Vector2i &p_sector) {
    hmap_t **ptr = sector_minmax.getptr(p_sector);

    if (ptr) {
        return *ptr;
    } else {
        _add_request(p_sector, DATA_TYPE_MINMAX, 0);
        return nullptr;
    }
}

void MapStorage::get_minmax(uint16_t p_x, uint16_t p_z, int p_lod, uint16_t &r_min, uint16_t &r_max) const {

}

void MapStorage::allocate_minmax(int p_sector_chunks, int p_lods, const Vector2i &p_world_regions, const Vector3 &p_map_scale, real_t p_far_view) {
    sector_size = p_sector_chunks;
    lods = p_lods;
    map_scale = p_map_scale;
    int sector_cells = sector_size * chunk_size;
    real_t sector_world_size_x = sector_cells * map_scale.x;
    real_t sector_world_size_z = sector_cells * map_scale.z;
    size_t blocks_x = Math::ceil(2.0 * p_far_view / sector_world_size_x) + 1;
    size_t blocks_z = Math::ceil(2.0 * p_far_view / sector_world_size_z) + 1;
    size_t block_count = blocks_x * blocks_z;
    minmax_lod_offsets.resize(lods);
    size_t block_size = 0;
    size_t lod_block_size = 2 * sector_size * sector_size;
    cancelled_frame = current_frame++;

    for (int ilod = 0; ilod < lods; ++ilod) {
        minmax_lod_offsets.set(ilod, block_size);
        block_size += lod_block_size;
        lod_block_size >>= 2;
    }

    if (minmax_buffer) {
        if (minmax_buffer->get_block_size() != block_size) {
            if (minmax_buffer->get_block_count() != block_count) {
                memdelete(minmax_buffer);
                minmax_buffer = nullptr;
                sector_minmax.clear();
            }

            if (!minmax_read.is_empty()) {
                minmax_read.clear();
            }
        }
    }

    if (!minmax_buffer) {
        // TODO: If sector_size < region_size, take into account the extra space needed to allocate full regions.
        minmax_buffer = memnew(BufferPool<uint16_t>(block_size, block_count));
    }

    if (minmax_read.is_empty() && sector_size != region_size) {
        int read_size = lod_expand(region_size, MIN(lods, saved_lods));
        minmax_read.resize(read_size);
    }
}

void MapStorage::update_viewer(const Vector3 &p_viewer_pos, const Vector3 &p_viewer_vel, const Vector3 &p_viewer_forward) {
    viewer_pos = p_viewer_pos;
    viewer_vel = p_viewer_vel;
    viewer_forward = p_viewer_forward;
    predicted_viewer_pos = viewer_pos + viewer_vel * PRIORITY_PREDICTION_DELTA_TIME;
}

void MapStorage::stop_io() {
    if (io_thread.is_started()) {
        io_running.clear();
        cancelled_frame = current_frame;
        io_pending.clear();

        while (io_queue->front()) {
            io_queue->pop();
        }

        io_thread.wait_to_finish();
    }
}

bool MapStorage::is_directory_set() const {
    return directory_path.is_empty() ? false : DirAccess::exists(directory_path);
}

void MapStorage::set_directory_path(const String &p_path) {
    directory_path = p_path;
    _clear();
    emit_signal(path_changed);
}

String MapStorage::get_directory_path() const {
    return directory_path;
}

void MapStorage::set_chunk_size(int p_size) {
    if (!size_locked) {
        ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain chunk size must be greater than zero.");
        ERR_FAIL_COND_EDMSG(p_size > MAX_CHUNK_SIZE, vformat("Terrain chunk size must be at most %d.", MAX_CHUNK_SIZE));
        const int size = round_po2(p_size, chunk_size);

        if (size != chunk_size) {
            chunk_size = size;
            _clear();
            emit_changed();
        }
    }
}

int MapStorage::get_chunk_size() const {
    return chunk_size;
}

void MapStorage::set_region_size(int p_size) {
    if (!size_locked) {
        ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain region size must be greater than zero.");
        const int size = round_po2(p_size, region_size);

        if (size != region_size) {
            region_size = size;
            saved_lods = MIN((int)Math::log2(float(region_size)) + 1, MAX_LOD_LEVELS);
            _clear();
            emit_changed();
        }
    }
}

int MapStorage::get_region_size() const {
    return region_size;
}

void MapStorage::set_size_locked(bool p_locked) {
    size_locked = p_locked;
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
    default_height = MIN(p_height, HMAP_HOLE_VALUE - 2);
}

bool MapStorage::_set(const StringName &p_name, const Variant &p_value) {
    String prop_name = p_name;

    if (prop_name == "chunk_size") {
        set_chunk_size(p_value);
        return true;
    } else if (prop_name == "region_size") {
        set_region_size(p_value);
        return true;
    }

    return false;
}

bool MapStorage::_get(const StringName &p_name, Variant &r_ret) const {
    String prop_name = p_name;

    if (prop_name == "chunk_size") {
        r_ret = get_chunk_size();
        return true;
    } else if (prop_name == "region_size") {
        r_ret = get_region_size();
        return true;
    }

    return false;
}

void MapStorage::_get_property_list(List<PropertyInfo> *p_list) const {
    int usage = PROPERTY_USAGE_DEFAULT | (size_locked ? PROPERTY_USAGE_READ_ONLY : PROPERTY_HINT_NONE);
    p_list->push_back(PropertyInfo(Variant::INT, "chunk_size", PROPERTY_HINT_RANGE, "1,256,1", usage));
    p_list->push_back(PropertyInfo(Variant::INT, "region_size", PROPERTY_HINT_RANGE, "1,256,1", usage));
    p_list->push_back(PropertyInfo(Variant::BOOL, "locked", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
}


void MapStorage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_directory_path", "path"), &MapStorage::set_directory_path);
	ClassDB::bind_method(D_METHOD("get_directory_path"), &MapStorage::get_directory_path);
    ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &MapStorage::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &MapStorage::get_chunk_size);
    ClassDB::bind_method(D_METHOD("set_region_size", "size"), &MapStorage::set_region_size);
	ClassDB::bind_method(D_METHOD("get_region_size"), &MapStorage::get_region_size);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "directory_path", PROPERTY_HINT_DIR), "set_directory_path", "get_directory_path");

    ADD_SIGNAL(MethodInfo(path_changed));
}

void MapStorage::_clear() {
    sector_minmax.clear();

    for (KeyValue<Vector2i, Region*> &kv : regions) {
        Region *region = kv.value;
        memdelete(region->header);
        memdelete(region);
    }

    if (minmax_buffer) {
        memdelete(minmax_buffer);
    }
}

void MapStorage::_process_requests(void *p_storage) {
    MapStorage *storage = static_cast<MapStorage *>(p_storage);

    while (storage->io_running.is_set()) {
        if (storage->io_queue->front()) {
            IORequest *request = storage->io_queue->front();

            if (request->data_type == DATA_TYPE_MINMAX) {
                storage->_load_sector_minmax(request->chunk);
            }

            storage->io_queue->pop();
        }
    }
}

void MapStorage::_add_request(const Vector2i &p_chunk, uint16_t p_data_type, uint16_t p_lod) {
    io_pending.push_back({p_chunk, current_frame, current_request++, p_data_type, p_lod});
}

void MapStorage::_submit_requests() {
    if (io_queue->size() == MAX_QUEUE_SIZE || io_pending.is_empty()) {
        return;
    }

    for (IORequest &request : io_pending) {
        if (request.data_type == DATA_TYPE_MINMAX) {
            const Vector3 p = Vector3(request.chunk.x * sector_size * map_scale.x, 0.0, request.chunk.y * sector_size * map_scale.z);
            request.priority = PRIORITY_MINMAX * _calc_request_priority(p, true);
        }
    }

    io_pending.sort_custom<RequestCompare>();
    int submitted = 0;
    int ipending = io_pending.size() - 1;

    while (ipending >= 0 && submitted < MAX_QUEUE_SIZE) {
        if (io_queue->try_push(io_pending[ipending])) {
            ipending--;
            submitted++;
        } else {
            break;
        }
    }

    io_pending.resize(ipending + 1);

    if (!io_thread.is_started()) {
        io_running.set();
        io_thread.start(_process_requests, this);
    }
}

void MapStorage::_load_region_minmax(const Vector2i &p_region_key, hmap_t *p_buffer, size_t p_size) {
    Region **region_ptr = regions.getptr(p_region_key);
    Region *region = nullptr;

    if (region_ptr) {
        region = *region_ptr;
    } else {
        _create_region(p_region_key, region);
    }

    if (region->header->has_minmax()) {
        region->data_access->seek(MINMAX_OFFSET);
        size_t nbytes = p_size * sizeof(hmap_t);
        int64_t len = region->data_access->get_buffer(reinterpret_cast<uint8_t*>(p_buffer), nbytes);
        ERR_FAIL_COND_EDMSG(len != nbytes, "Returned buffer of different size than expected.");
    } else {
        hmap_t hmax = default_height + 1;

        for (int i = 0; i < p_size; i += 2) {
            p_buffer[i] = default_height;
            p_buffer[i + 1] = hmax;
        }
    }
}

void MapStorage::_load_sector_minmax(const Vector2i &p_sector) {
    if (sector_size < region_size) {
        int region_sectors = region_size / sector_size;
        Vector2i region_key = p_sector / region_sectors;
        uint16_t *src = minmax_read.ptrw();
        _load_region_minmax(region_key, src, minmax_read.size());

        for (int izs = 0; izs < region_sectors; ++izs) {
            const int z_sector = izs + region_key.y * region_sectors;

            for (int ixs = 0; ixs < region_sectors; ++ixs) {
                const int x_sector = ixs + region_key.x * region_sectors;
                const Vector2i sector_key = Vector2i(x_sector, z_sector);
                int write_size = 2 * sector_size;
                hmap_t *sector_buffer = minmax_buffer->allocate(current_frame, sector_key);
                ERR_FAIL_NULL_EDMSG(sector_buffer, "Error allocating buffer to read minmax data.");
                sector_minmax[sector_key] = sector_buffer;
                int src_lod_offset = 0;
                int src_block_size = 2 * region_size * region_size;

                for (int ilod = 0; ilod < lods; ++ilod) {
                    const int src_offset = src_lod_offset + ixs * write_size + izs * write_size * write_size * region_sectors;

                    for (int iz = 0; iz < write_size; ++iz) {
                        const int src_index = src_offset + iz * write_size * region_sectors;
                        memcpy(sector_buffer, src + src_index, write_size);
                        sector_buffer += write_size;
                    }

                    src_lod_offset += src_block_size;
                    src_block_size >>= 2;
                    write_size >>= 1;
                }
            }
        }
    } else {
        hmap_t *sector_buffer = minmax_buffer->allocate(current_frame, p_sector);
        ERR_FAIL_NULL_EDMSG(sector_buffer, "Error allocating buffer to read minmax data.");
        sector_minmax[p_sector] = sector_buffer;

        if (sector_size == region_size) {
            _load_region_minmax(p_sector, sector_buffer, minmax_buffer->get_block_size());
        } else { // sector_size > region_size
            int sector_regions = sector_size / region_size;
            int num_lods = MIN(saved_lods, lods);

            for (int izr = 0; izr < sector_regions; ++izr) {
                const int z_region = izr + p_sector.y * sector_regions;

                for (int ixr = 0; ixr < sector_regions; ++ixr) {
                    const int x_region = ixr + p_sector.x * sector_regions;
                    const Vector2i region_key = Vector2i(x_region, z_region);
                    uint16_t *data = minmax_read.ptrw();
                    _load_region_minmax(region_key, data, minmax_read.size());
                    int read_size = 2 * region_size;

                    for (int ilod = 0; ilod < num_lods; ++ilod) {
                        const int lod_offset = minmax_lod_offsets[ilod];
                        const int region_offset = read_size * ixr + izr * read_size * read_size * sector_regions;

                        for (int iz = 0; iz < read_size; ++iz) {
                            const int buffer_index = lod_offset + region_offset + iz * read_size * sector_regions;
                            memcpy(sector_buffer + buffer_index, data, read_size);
                            data += read_size;
                        }

                        read_size >>= 1;
                    }
                }
            }

            {
                // Fill in remaining LODs.
                int size = sector_size >> num_lods;

                for (int ilod = num_lods; ilod < lods; ++ilod) {
                    const int src_lod_offset = minmax_lod_offsets[ilod - 1];
                    const int dst_lod_offset = minmax_lod_offsets[ilod];

                    for (int iz = 0; iz < size; ++iz) {
                        for (int ix = 0; ix < size; ++ix) {
                            const int src_index = src_lod_offset + 4 * (ix + 2 * iz * size);
                            const int dst_index = dst_lod_offset + 2 * (ix + iz * size);
                            const hmap_t src_min1 = sector_buffer[src_index];
                            const hmap_t src_max1 = sector_buffer[src_index + 1];
                            const hmap_t src_min2 = sector_buffer[src_index + 2];
                            const hmap_t src_max2 = sector_buffer[src_index + 3];
                            const hmap_t src_min3 = sector_buffer[src_index + 4 * size];
                            const hmap_t src_max3 = sector_buffer[src_index + 4 * size + 1];
                            const hmap_t src_min4 = sector_buffer[src_index + 4 * size + 2];
                            const hmap_t src_max4 = sector_buffer[src_index + 4 * size + 3];
                            sector_buffer[dst_index] = MIN(src_min1, MIN(src_min2, MIN(src_min3, src_min4)));
                            sector_buffer[dst_index + 1] = MAX(src_max1, MAX(src_max2, MAX(src_max3, src_max4)));
                        }
                    }

                    size >>= 1;
                }
            }
        }
    }
}

void MapStorage::_create_region(const Vector2i &p_region_key, Region *r_region) {
    r_region = memnew(Region);
    Header *header = memnew(Header);
    memset(header, 0, HEADER_SIZE);
    header->version = FORMAT_VERSION;
    r_region->header = header;

    regions[p_region_key] = r_region;
}

float MapStorage::_calc_request_priority(const Vector3 &p_chunk_pos, bool p_in_frustum) {
    float distance = viewer_pos.distance_to(p_chunk_pos);
    float predicted_distance = predicted_viewer_pos.distance_to(p_chunk_pos);
    float effective_dist = MIN(distance, predicted_distance);
    float priority = PRIORITY_DISTANCE_FACTOR * PRIORITY_DISTANCE_HALF_DECAY / (effective_dist + PRIORITY_DISTANCE_HALF_DECAY);

    if (p_in_frustum) {
        priority *= PRIORITY_IN_FRUSTUM;
    }

    Vector3 to_chunk = (p_chunk_pos - viewer_pos).normalized();
    float dot = to_chunk.dot(viewer_forward);

    if (dot < 0.0) {
        // Behind camera, heavily reduce priority.
        priority *= 0.1f;
    } else {
        // Ahead, bonus based on alignment.
        priority *= (1.0f + dot * 0.5f);
    }

    return priority;
}

bool MapStorage::_is_format_correct(Ref<FileAccess> &p_file) const {
    constexpr int size = MAGIC_SIZE + 1;
    uint8_t top[size];
    p_file->get_buffer(top, size);

    for (int i = 0; i < MAGIC_SIZE; ++i) {
        if (top[i] != MAGIC_STRING[i]) {
            return false;
        }
    }

    uint8_t endianess = top[MAGIC_SIZE];

    if (endianess == FORMAT_LITTLE_ENDIAN) {
        p_file->big_endian = false;
    } else if (endianess == FORMAT_BIG_ENDIAN) {
        p_file->big_endian = true;
    } else {
        return false;
    }

    p_file->seek(0);
    return true;
}

MapStorage::MapStorage() {
    io_queue = memnew(SPSCQueue<IORequest>(MAX_QUEUE_SIZE));
    // result_queue = memnew(SPSCQueue<IOResult>(MAX_QUEUE_SIZE));
}

MapStorage::~MapStorage() {
    _clear();
    stop_io();

    memdelete(io_queue);
    // memdelete(result_queue);

    // if (pools_ready) {
    //     memdelete(height_pool);
    //     memdelete(splat_pool);
    // }
}