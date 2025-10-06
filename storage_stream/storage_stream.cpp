/**
 * storage_stream.cpp
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#include "storage_stream.h"

static const String MAIN_FILE_EXT = "sdesc";
static const String DATA_FILE_EXT = "sdat";

Error TStorageStream::create(const String p_path, int p_block_size, int p_chunk_size) {
    ERR_FAIL_COND_V_EDMSG(p_path.get_extension() != MAIN_FILE_EXT, ERR_FILE_BAD_PATH, vformat("Incorrect Stream Storage description file extension. It must have .%s extension.", MAIN_FILE_EXT));
    block_size = p_block_size;
    chunk_size = p_chunk_size;
    String desc_file = p_path.get_file();
    String base_name = desc_file.get_basename();
    String directory = p_path.get_base_dir();
    Error error;
    Ref<DirAccess> dir = DirAccess::open(directory, &error);
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Error while opening Stream Storage directory.");
    error = dir->list_dir_begin();
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Can`t iterate over files in Stream Storage directory.");
    String file_name = dir->get_next();
    PackedStringArray to_delete;

    while (!file_name.is_empty()) {
        if (!dir->current_is_dir()) {
            if (file_name.begins_with(base_name)) {
                if (file_name == desc_file) {
                    WARN_PRINT_ED(vformat("Stream Storage description file %s already exists. It will be overwritten.", file_name));
                } else if (file_name.get_extension() == DATA_FILE_EXT) {
                    String remaining = file_name.right(-desc_file.length());
                    PackedStringArray parts = remaining.split("_", false);

                    if (parts.size() == 2) {
                        WARN_PRINT_ED(vformat("Stream Storage data file %s already exist. It will be removed.", file_name));
                        to_delete.push_back(file_name);
                    }
                }
            }
        }
    }

    // Remove existing data files.
    for (int i = 0; i < to_delete.size(); ++i) {
        dir->remove(to_delete[i]);
    }

    dir->list_dir_end();

    // Write description file header.
    desc = FileAccess::open(p_path, FileAccess::WRITE_READ, &error);
    ERR_FAIL_COND_V_EDMSG(error != OK, error, vformat("Can't create stream description file %s.", p_path));
    version = FORMAT_VERSION;
    desc->store_32(version);
    desc->store_32(block_size);
    desc->store_32(chunk_size);

    return OK;
}

Error TStorageStream::load(const String p_path) {
    ERR_FAIL_COND_V_EDMSG(p_path.get_extension() != MAIN_FILE_EXT, ERR_FILE_BAD_PATH, vformat("Incorrect Stream Storage description file extension. It must have .%s extension.", MAIN_FILE_EXT));
    String desc_file = p_path.get_file();
    String base_name = desc_file.get_basename();
    String directory = p_path.get_base_dir();
    Error error;
    Ref<DirAccess> dir = DirAccess::open(directory, &error);
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Error while opening Stream Storage directory.");
    error = dir->list_dir_begin();
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Can`t iterate over files in Stream Storage directory.");
    String file_name = dir->get_next();

    while (!file_name.is_empty()) {
        if (!dir->current_is_dir()) {
            if (file_name.begins_with(base_name)) {
                if (file_name == base_name) {
                    desc = FileAccess::open(p_path, FileAccess::READ_WRITE, &error);
                    ERR_FAIL_COND_V_EDMSG(error != OK, error, vformat("Can't open stream description file %s.", p_path));
                } else if (file_name.get_extension() == DATA_FILE_EXT) {
                    String remaining = file_name.get_basename().right(-desc_file.length());
                    PackedStringArray parts = remaining.split("_", false);

                    if (parts.size() == 2) {
                        int x = parts[0].to_int();
                        int z = parts[1].to_int();
                        String data_file = directory.path_join(file_name);
                        Ref<FileAccess> file = FileAccess::open(data_file, FileAccess::READ_WRITE, &error);
                        ERR_CONTINUE_EDMSG(error != OK, vformat("Can`t open stream data file %s.", data_file));
                        blocks[Vector2i(x, z)] = _create_block(file);
                    }
                }
            }
        }
    }

    dir->list_dir_end();

    if (desc.is_null()) {
        _clear_blocks();
        ERR_FAIL_V_EDMSG(ERR_FILE_NOT_FOUND, "Stream description file not found.");
    }

    version = desc->get_32();
    ERR_FAIL_COND_V_EDMSG(version > FORMAT_VERSION, ERR_INVALID_DATA, "Incorrect description file version.");
    block_size = desc->get_32();
    chunk_size = desc->get_32();
}

void TStorageStream::close() {
    if (desc.is_valid() && desc->is_open()) {
        desc->close();
    }

    _clear_blocks();
}

const TStorageStream::Block *TStorageStream::_get_block(int x, int y) const {

}

void TStorageStream::_clear_blocks() {
    for (KeyValue<Vector2i, Block*> &kv : blocks) {
        kv.value->chunk_positions.clear();
        kv.value->file->close();
        memdelete(kv.value);
    }

    blocks.clear();
}

TStorageStream::Block *TStorageStream::_create_block(Ref<FileAccess> p_file) const {
    Block *block = memnew(Block);
    block->file = p_file;
    HashMap<Vector2i, uint64_t> &map = block->chunk_positions;

    for (int iz = 0; iz < block_size; ++iz) {
        for (int ix = 0; ix < block_size; ++ix) {
            map[Vector2i(ix, iz)] = p_file->get_64();
        }
    }

    return block;
}

Vector2i TStorageStream::_get_block_id(int p_x, int p_z) const {

}

TStorageStream::TStorageStream() {
}

TStorageStream::~TStorageStream() {
    close();
}