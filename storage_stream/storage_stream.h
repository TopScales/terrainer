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
#include "core/io/resource.h"

class TStorageStream : public Resource {
    GDCLASS(TStorageStream, Resource);

protected:
    static const uint32_t FORMAT_VERSION = 1;

    struct Block {
        uint8_t version;
        uint8_t gaps;
        Ref<FileAccess> file;

        Block(Ref<FileAccess> p_file, uint8_t p_version, uint8_t p_gaps) : file(p_file), version(p_version), gaps(p_gaps) {}
    };

private:
    int block_size = 32;
    bool directory_use_custom = false;
    String directory_path;
    HashMap<Vector2i, Block *> blocks;

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    static void _bind_methods();

public:
    Error load_headers();
    void clear();

    void set_block_size(int p_size);
    int get_block_size() const;
    void set_directory_use_custom(bool p_use_custom);
    bool is_directory_use_custom() const;
    void set_directory_path(String p_path);
    String get_directory_path() const;

    TStorageStream();
    ~TStorageStream();
};

#endif // TERRAINER_STORAGE_STREAM_H