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
                Ref<FileAccess> file = FileAccess::open(file_path, mode, &error);
                ERR_CONTINUE_EDMSG(error != OK, vformat("Can`t open stream region file %s.", file_path));
                char unsigned magic[MAGIC_SIZE];
                file->get_buffer(magic, MAGIC_SIZE);
                error = file->get_error();
                ERR_CONTINUE_EDMSG(error != OK, vformat("Error (%d) while reading ragion file %s.", error, file_path));
                ERR_CONTINUE_EDMSG(!_is_format_correct(file), vformat("Region file %s has incorrect format.", file_path));
                HeaderBytes *hb = memnew(HeaderBytes);
                file->get_buffer(hb->bytes, HEADER_SIZE);
                ERR_CONTINUE_EDMSG(hb->value.chunk_size != chunk_size, vformat("Wrong chunk size in region file %s.", file_name));
                ERR_CONTINUE_EDMSG(hb->value.region_size != region_size, vformat("Wrong region size in region file %s.", file_name));
                Region *region = memnew(Region);
                region->data_access = FileAccess::open(file_path, mode, &error);
                ERR_CONTINUE_EDMSG(error != OK, vformat("Can`t open stream region data file %s.", file_path));
                region->data_access->big_endian = file->big_endian;
                region->header = &hb->value;
                region->query_access = file;
                regions[Vector2i(x, z)] = region;
            }
        }

        file_name = dir->get_next();
    }

    dir->list_dir_end();
    return OK;
}

void MapStorage::clear() {
    for (KeyValue<Vector2i, Region*> &kv : regions) {
        Region *region = kv.value;
        memdelete(region->header);
        memdelete(region);
    }
}

bool MapStorage::is_directory_set() const {
    return directory_path.is_empty() ? false : DirAccess::exists(directory_path);
}

void MapStorage::set_directory_path(String p_path) {
    directory_path = p_path;
    clear();
    emit_signal(path_changed);
}

String MapStorage::get_directory_path() const {
    return directory_path;
}

void MapStorage::set_chunk_size(int p_size) {
    if (!size_locked) {
        chunk_size = p_size;
        emit_changed();
    }
}

int MapStorage::get_chunk_size() const {
    return chunk_size;
}

void MapStorage::set_region_size(int p_size) {
    if (!size_locked) {
        region_size = p_size;
        emit_changed();
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

void MapStorage::set_sector_size(int p_size) {
    sector_size = p_size;
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
    result_queue = memnew(SPSCQueue<IOResult>(MAX_QUEUE_SIZE));
}

MapStorage::~MapStorage() {
    clear();

    memdelete(io_queue);
    memdelete(result_queue);

    if (pools_ready) {
        memdelete(height_pool);
        memdelete(splat_pool);
    }
}