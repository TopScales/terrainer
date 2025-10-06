/**
 * storage_stream.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_STORAGE_STREAM_H
#define TERRAINER_STORAGE_STREAM_H

#include "core/io/file_access.h"
#include "core/io/dir_access.h"

class TStorageStream : public RefCounted {
    GDCLASS(TStorageStream, RefCounted);

protected:
    static const uint32_t FORMAT_VERSION = 1;

    struct Block {
        Ref<FileAccess> file;
    };

    Ref<FileAccess> desc;
    HashMap<Vector2i, Block *> blocks;
    uint32_t version = 0;
    int block_size = 128;
    int chunk_size = 32;

    _FORCE_INLINE_ const Block *_get_block(int x, int z) const;

private:
    static const String MAIN_FILE_EXT;
    static const String DATA_FILE_EXT;

    void _clear_blocks();
    virtual Block *_create_block(Ref<FileAccess> p_file) const;
    _FORCE_INLINE_ Vector2i _get_block_id(int p_x, int p_z) const;

public:
    Error create(const String p_path, int p_block_size, int p_chunk_size);
    Error load(const String p_path);
    void close();

    void load_chunk(PackedByteArray &p_buffer);

    TStorageStream();
    ~TStorageStream();
};

#endif // TERRAINER_STORAGE_STREAM_H