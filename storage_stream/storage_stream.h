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
public:
    enum SegmentSize : unsigned int {
		SEGMENT_1K,
		SEGMENT_2K,
		SEGMENT_4K,
		SEGMENT_8K,
		SEGMENT_MAX
	};

private:
    static const uint32_t FORMAT_VERSION = 1;
    static const uint32_t MAX_BLOCK_SIZE = 1 << 10;
    static const uint32_t HEADER_SIZE = 4;

    struct Region {
        Vector<PackedByteArray> data_set;
        uint8_t format = 0;
        uint16_t file_segments = 0;
    };

    struct Block {
        uint8_t version = 0;

        union {
            struct {
                unsigned int block_size_shift : 4;
                bool bit_flag_3 : 1;
                bool uniform_sections : 1;
                SegmentSize segment_size : 2;
            } v;
            uint8_t byte = 0;
        } flags;

        uint8_t source = 0;
        uint8_t gaps = 0;
        Ref<FileAccess> file;
        HashMap<Vector2i, Region *> regions;

        Block(Ref<FileAccess> p_file, uint8_t p_version, uint8_t p_gaps) : file(p_file), version(p_version), gaps(p_gaps) {}
    };

    int block_size = 32;
    SegmentSize segment_size = SEGMENT_4K;
    bool directory_use_custom = false;
    String directory_path;
    HashMap<Vector2i, Block *> blocks;

    void _load_region(Block *p_block, int p_region_id, Region *r_region);

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