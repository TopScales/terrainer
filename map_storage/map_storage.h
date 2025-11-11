/**
 * map_storage.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_MAP_STORAGE_H
#define TERRAINER_MAP_STORAGE_H

#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "minmax_map.h"

class TMapStorage : public Resource {
    GDCLASS(TMapStorage, Resource);

public:
    static const uint64_t HEADER_SIZE = 8;
    enum SegmentSize : unsigned int {
        SEGMENT_1B,
		SEGMENT_1K,
		SEGMENT_4K,
		SEGMENT_32K,
		SEGMENT_MAX
	};

private:
    static const String DATA_FILE_EXT;
    static const String DEFAULT_BLOCK_FILE_NAME;
    static const uint8_t FORMAT_VERSION = 1;
    static const uint32_t MAX_BLOCK_CELLS = 1 << 14;
    static const uint32_t SEGMENTS[4];

    struct Block {
        uint8_t version = 0;
        uint16_t format = 0;
        uint16_t splat_addr = 0;
        uint16_t meta_addr = 0;
        Ref<FileAccess> file;
        PackedByteArray heightmap;
        PackedByteArray splat_map;

        _FORCE_INLINE_ static uint8_t format_to_segment_size(uint16_t p_format) { return (p_format >> 8) & 0x000F; };
        _FORCE_INLINE_ bool has_heightmap() const { return format & 0x0010; }
        _FORCE_INLINE_ bool has_splat_map() const { return format & 0x0020; }
        _FORCE_INLINE_ bool has_metada() const { return format & 0x0040; }
        _FORCE_INLINE_ uint8_t get_max_lod() const { return format & 0x000F; }
        _FORCE_INLINE_ uint8_t get_segment_size() const { return format_to_segment_size(format); }

        Block(Ref<FileAccess> &p_file, uint8_t p_version, uint16_t p_format, uint16_t p_splat_addr, uint16_t p_meta_addr) :
            file(p_file), version(p_version), format(p_format), splat_addr(p_splat_addr), meta_addr(p_meta_addr) {}
    };

    bool directory_use_custom = false;
    String directory_path;
    TWorldInfo world_info;
    HashMap<Vector2i, Block *> blocks;
    SegmentSize segment_size = SEGMENT_1K;

    TMinmaxMap minmax_map;

    void _select_segment_size();

protected:
    void _notification(int p_what);
    bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    static void _bind_methods();

public:
    Error load_headers();
    void load_minmax_data();
    void clear();

    void set_chunk_size(int p_size);
    int get_chunk_size() const;
    void set_block_size(int p_size);
    int get_block_size() const;
    void set_world_blocks(const Vector2i &p_blocks);
    Vector2i get_world_blocks() const;
    void set_directory_use_custom(bool p_use_custom);
    bool is_directory_use_custom() const;
    void set_directory_path(String p_path);
    String get_directory_path() const;
    TWorldInfo *get_world_info();

    TMapStorage();
    ~TMapStorage();
};

#endif // TERRAINER_MAP_STORAGE_H