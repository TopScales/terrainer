/**
 * map_storage.cpp
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#include "map_storage.h"

#include "core/io/dir_access.h"

const String TMapStorage::DATA_FILE_EXT = "block";
const String TMapStorage::DEFAULT_BLOCK_FILE_NAME = "map";
const uint32_t TMapStorage::SEGMENTS[4] = {1u, 1u << 10u, 1u << 12u, 1u << 15u};

Error TMapStorage::load_headers() {
    String base_name;
    String directory;

    if (directory_use_custom) {
        ERR_FAIL_COND_V_EDMSG(!DirAccess::dir_exists_absolute(directory_path), ERR_FILE_BAD_PATH, vformat("Directory %s doesn't exist.", directory_path));
        base_name = DEFAULT_BLOCK_FILE_NAME;
        directory = directory_path;
    } else {
        base_name = get_path().get_file().get_basename();
        directory = get_path().get_base_dir();
    }

    Error error;
    Ref<DirAccess> dir = DirAccess::open(directory, &error);
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Error while opening MapStorage directory.");
    error = dir->list_dir_begin();
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Can`t iterate over files in MapStorage directory.");
    String file_name = dir->get_next();

    while (!file_name.is_empty()) {
        if (!dir->current_is_dir() && file_name.begins_with(base_name) && file_name.get_extension() == DATA_FILE_EXT) {
            String remaining = file_name.get_basename().right(-base_name.length());
            PackedStringArray parts = remaining.split("_", false);

            if (parts.size() == 2) {
                int x = parts[0].to_int();
                int z = parts[1].to_int();
                String block_file = directory.path_join(file_name);
                Ref<FileAccess> file = FileAccess::open(block_file, FileAccess::READ_WRITE, &error);
                ERR_CONTINUE_EDMSG(error != OK, vformat("Can`t open stream block file %s.", block_file));
                uint8_t version = file->get_8();
                ERR_CONTINUE_EDMSG(version > FORMAT_VERSION, vformat("Wrong format version in block file %s.", file_name));
                uint8_t shifts = file->get_8();
                int csz = 1 << (shifts & 0x0F);
                ERR_CONTINUE_EDMSG(csz != chunk_size, vformat("Incorrect chunk size in block file %s.", file_name));
                int bsz = 1 << ((shifts & 0xF0) >> 4);
                ERR_CONTINUE_EDMSG(bsz != block_size, vformat("Incorrect block size in block file %s.", file_name));
                uint16_t format = file->get_16();
                ERR_CONTINUE_EDMSG(Block::format_to_segment_size(format) != segment_size, vformat("Incorrect segment size in block file %s.", file_name));
                uint16_t splat_map_addr = file->get_16();
                uint16_t meta_addr = file->get_16();
                Block *block = memnew(Block(file, version, format, splat_map_addr, meta_addr));
                blocks[Vector2i(x, z)] = block;
            }
        }
    }

    dir->list_dir_end();
    return OK;
}

void TMapStorage::load_minmax_data() {
}

void TMapStorage::clear() {
    for (KeyValue<Vector2i, Block*> &kv : blocks) {
        Block *block = kv.value;
        block->file->close();
        block->heightmap.clear();
        block->splat_map.clear();
        memdelete(block);
    }

    blocks.clear();
}

void TMapStorage::set_chunk_size(int p_size) {
    ERR_FAIL_COND_EDMSG(p_size * block_size > MAX_BLOCK_CELLS, "Number of block cells exceeded.");

    if (p_size != chunk_size) {
        chunk_size = p_size;
        _select_segment_size();
        emit_changed();
    }
}

int TMapStorage::get_chunk_size() const {
    return chunk_size;
}

void TMapStorage::set_block_size(int p_size) {
    ERR_FAIL_COND_EDMSG(p_size * chunk_size > MAX_BLOCK_CELLS, "Number of block cells exceeded.");

    if (p_size != block_size) {
        block_size = p_size;
        _select_segment_size();
        emit_changed();
    }
}

int TMapStorage::get_block_size() const {
    return block_size;
}

void TMapStorage::set_directory_use_custom(bool p_use_custom) {
    if (!is_built_in()) {
        directory_use_custom = p_use_custom;
    }
}

bool TMapStorage::is_directory_use_custom() const {
    return directory_use_custom;
}

void TMapStorage::set_directory_path(String p_path) {
    directory_path = p_path;
}

String TMapStorage::get_directory_path() const {
    if (directory_use_custom) {
        return directory_path;
    } else {
        return get_path().get_base_dir();
    }
}

void TMapStorage::_notification(int p_what) {
    if (p_what == NOTIFICATION_POSTINITIALIZE) {
        if (is_built_in()) {
            directory_use_custom = true;
        }
    }
}

bool TMapStorage::_set(const StringName &p_name, const Variant &p_value) {
    String prop_name = p_name;

    if (prop_name == "directory_use_custom") {
        set_directory_use_custom(p_value);
    } else if (prop_name == "directory_path") {
        set_directory_path(p_value);
    } else {
        return false;
    }

    return true;
}

bool TMapStorage::_get(const StringName &p_name, Variant &r_ret) const {
    String prop_name = p_name;

    if (prop_name == "directory_use_custom") {
        r_ret = is_directory_use_custom();
    } else if (prop_name == "directory_path") {
        r_ret = get_directory_path();
    } else {
        return false;
    }

    return true;
}

void TMapStorage::_get_property_list(List<PropertyInfo> *p_list) const {
    p_list->push_back(PropertyInfo(Variant::NIL, "Custom Directory", PROPERTY_HINT_NONE, "directory_", PROPERTY_USAGE_GROUP));
    uint32_t usage = PROPERTY_USAGE_DEFAULT | (is_built_in() ? PROPERTY_USAGE_READ_ONLY : 0);
    p_list->push_back(PropertyInfo(Variant::BOOL, "directory_use_custom", PROPERTY_HINT_GROUP_ENABLE, "", usage));
    p_list->push_back(PropertyInfo(Variant::STRING, "directory_path", PROPERTY_HINT_DIR));
}

void TMapStorage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &TMapStorage::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &TMapStorage::get_chunk_size);
    ClassDB::bind_method(D_METHOD("set_block_size", "size"), &TMapStorage::set_block_size);
	ClassDB::bind_method(D_METHOD("get_block_size"), &TMapStorage::get_block_size);
    ClassDB::bind_method(D_METHOD("set_directory_use_custom", "use_custom"), &TMapStorage::set_directory_use_custom);
	ClassDB::bind_method(D_METHOD("is_directory_use_custom"), &TMapStorage::is_directory_use_custom);
    ClassDB::bind_method(D_METHOD("set_directory_path", "path"), &TMapStorage::set_directory_path);
	ClassDB::bind_method(D_METHOD("get_directory_path"), &TMapStorage::get_directory_path);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size", PROPERTY_HINT_RANGE, "1,256,1"), "set_chunk_size", "get_chunk_size");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "block_size", PROPERTY_HINT_RANGE, "1,256,1"), "set_block_size", "get_block_size");
}

void TMapStorage::_select_segment_size() {
    int total_size = chunk_size * block_size;

    if (total_size > (1 << 12)) {
        segment_size = SEGMENT_32K;
    } else if (total_size > (1 << 11)) {
        segment_size = SEGMENT_4K;
    } else if (total_size > (1 << 6)) {
        segment_size = SEGMENT_1K;
    } else {
        segment_size = SEGMENT_1B;
    }
}

TMapStorage::TMapStorage() {
}

TMapStorage::~TMapStorage() {
    clear();
}