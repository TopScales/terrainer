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
#include "core/object/worker_thread_pool.h"
#include "core/os/mutex.h"
#include "minmax_map.h"
#include "scene/resources/texture_rd.h"
// #include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_server.h"
#include "../terrain_info.h"

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
    static const uint32_t FLAG_HEIGHTMAP = 0x0010;
    static const uint32_t FLAG_SPLAT_MAP = 0x0020;
    static const uint32_t FLAG_METADATA = 0x0040;
    static const uint32_t SHIFT_MAX_LOD = 8;
    static const uint32_t SHIFT_MAX_MINMAX_LOD = 12;
    static const uint32_t MASK_MAX_LOD = 0x000F;
    static const int HEIGHTMAP_DATA_BYTES = 2;
    static const int EXTRA_BUFFER_LAYERS = 4;
    static const int UNLOAD_FRAME_TOLERANCE = 100;

    struct Block {
        Vector2i position;
        uint8_t version = 0;
        uint16_t format = 0;
        uint16_t splat_addr = 0;
        uint16_t meta_addr = 0;
        Ref<FileAccess> file;
        PackedByteArray heightmap;

        _FORCE_INLINE_ static uint16_t get_format(int p_lods, int p_minmax_lods, int p_segment, bool p_has_heightmap, bool p_has_splat_map, bool p_has_meta) {
            return p_segment | (p_has_heightmap ? FLAG_HEIGHTMAP : 0) | (p_has_splat_map ? FLAG_SPLAT_MAP : 0) | (p_has_meta ? FLAG_METADATA : 0) |
                (p_lods << SHIFT_MAX_LOD) | (p_minmax_lods << SHIFT_MAX_MINMAX_LOD);
        };
        _FORCE_INLINE_ static uint8_t format_to_segment_size(uint16_t p_format) { return p_format & 0x000F; };
        _FORCE_INLINE_ bool has_heightmap() const { return format & FLAG_HEIGHTMAP; }
        _FORCE_INLINE_ bool has_splat_map() const { return format & FLAG_SPLAT_MAP; }
        _FORCE_INLINE_ bool has_metada() const { return format & FLAG_METADATA; }
        _FORCE_INLINE_ uint8_t get_max_lod() const { return (format >> SHIFT_MAX_LOD) & MASK_MAX_LOD; }
        _FORCE_INLINE_ uint8_t get_max_minmax_lod() const { return (format >> SHIFT_MAX_MINMAX_LOD) & MASK_MAX_LOD; }
        _FORCE_INLINE_ uint8_t get_segment_size() const { return format_to_segment_size(format); }

        void clear() {
            file->close();
            heightmap.clear();
        }

        Block(const Vector2i &p_pos, Ref<FileAccess> &p_file, uint8_t p_version, uint16_t p_format, uint16_t p_splat_addr, uint16_t p_meta_addr) :
            position(p_pos), file(p_file), version(p_version), format(p_format), splat_addr(p_splat_addr), meta_addr(p_meta_addr) {}
        Block(const Vector2i &p_pos, uint8_t p_version, uint16_t p_format, uint16_t p_splat_addr, uint16_t p_meta_addr) :
            position(p_pos), version(p_version), format(p_format), splat_addr(p_splat_addr), meta_addr(p_meta_addr) {}
    };

    struct TextureData {
        int layer = 0;
        bool loaded = false;
        int jobs = 0;
        Mutex job_mutex;
        Vector<Block *> blocks;
        uint32_t frame = 0;
        // TODO: Neigh info
    };

    bool directory_use_custom = false;
    String directory_path;
    TWorldInfo world_info;
    int max_saved_lods = -1;
    int saved_lods = 10;
    HashMap<Vector2i, Block*> blocks;
    SegmentSize segment_size = SEGMENT_1K;
    HashMap<Vector2i, TextureData*> texture_data_map;
    int lod_levels = 1;
    int num_layers = 0;
    int used_layers = 0;
    Vector<int> mipmaps_offset;
    bool keep_data = false;
    int num_layer_blocks = 0;
    Mutex tex_data_mutex;
    Mutex tasks_mutex;
    HashSet<WorkerThreadPool::TaskID> tasks;
    Ref<Texture2DArrayRD> heightmap_texture;
    RID rd_heightmap_texture;
    uint32_t frame = 0;
    Vector<int> unused_layers;

    TMinmaxMap minmax_map;

    void _select_segment_size();
    void _set_saved_lods();
    void _request_texture_layer(const Vector2i &p_node_pos, TextureData *p_texture_data);
    void _load_block_data(const Vector2i &p_nopde_pos, int p_job_id);
    void _create_textures(int p_layers, int p_width, int p_height, int p_mipmaps);

protected:
    void _notification(int p_what);
    bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    static void _bind_methods();

public:
    Error load_headers();
    void setup(const TTerrainInfo &p_info);
    bool get_node_texture_layer(const Vector2i &p_node_pos, int &r_layer);
    void load_from_buffer(const PackedByteArray &p_heightmap);
    void arrange_layers();
    void clear();

    void set_chunk_size(int p_size);
    int get_chunk_size() const;
    void set_block_size(int p_size);
    int get_block_size() const;
    void set_world_blocks(const Vector2i &p_blocks);
    Vector2i get_world_blocks() const;
    void set_max_saved_lods(int p_max_lods);
    int get_max_saved_lods() const;
    void set_directory_use_custom(bool p_use_custom);
    bool is_directory_use_custom() const;
    void set_directory_path(String p_path);
    String get_directory_path() const;
    Ref<Texture2DArrayRD> get_heightmap_texture() const;

    TWorldInfo *get_world_info();
    const TMinmaxMap *get_minmax_map() const;

    TMapStorage();
    ~TMapStorage();
};

#endif // TERRAINER_MAP_STORAGE_H