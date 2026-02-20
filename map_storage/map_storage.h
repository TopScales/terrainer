/**
 * map_storage.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_MAP_STORAGE_H
#define TERRAINER_MAP_STORAGE_H

#include "buffer_pool.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/os/thread.h"
#include "queue.h"
#include "scene/resources/texture_rd.h"
#include "servers/rendering/rendering_server.h"

// #include "core/object/worker_thread_pool.h"
// #include "core/os/mutex.h"
// #include "minmax_map.h"
// #include "scene/resources/texture_rd.h"
// #include "servers/rendering/rendering_device_binds.h"
// #include "servers/rendering/rendering_server.h"
// #include "../terrain_info.h"

using namespace rigtorp;

namespace Terrainer {

class MapStorage : public Resource {
    GDCLASS(MapStorage, Resource);

    friend class Terrain;

public:
    typedef uint16_t hmap_t;

    union CellKey {
        struct {
            uint16_t x;
            uint16_t z;
        } cell;
        uint32_t key;

        constexpr CellKey() : key(0) {}
        constexpr CellKey(uint16_t p_x, uint16_t p_z) : cell({p_x, p_z}) {}

        constexpr CellKey operator+(CellKey p_k) const { return CellKey(cell.x + p_k.cell.x, cell.z + p_k.cell.z); }
        constexpr void operator+=(CellKey p_k) { cell.x += p_k.cell.x; cell.z += p_k.cell.z; }
        constexpr CellKey operator-(CellKey p_k) const { return CellKey(cell.x - p_k.cell.x, cell.z - p_k.cell.z); }
        constexpr void operator-=(CellKey p_k) { cell.x -= p_k.cell.x; cell.z -= p_k.cell.z; }
        constexpr bool operator==(CellKey p_k) const { return key == p_k.key; }
        constexpr bool operator!=(CellKey p_k) const { return key != p_k.key; }

        _FORCE_INLINE_ Vector3 position(real_t p_scale_x, real_t p_scale_z) const {
            return Vector3(cell.x * p_scale_x, 0.0, cell.z * p_scale_z);
        }

        uint32_t hash() const {
            return hash_murmur3_one_32(key);
	    }
    };
    static_assert(sizeof(CellKey) == 4);

    struct NodeKey {
        CellKey sector;
        CellKey cell;

        constexpr NodeKey() {}
        constexpr NodeKey(CellKey p_sector, CellKey p_cell) : sector(p_sector), cell(p_cell) {}
        constexpr bool operator==(const NodeKey &p_k) const { return sector == p_k.sector && cell == p_k.cell; }

        _FORCE_INLINE_ Vector3 sector_position(real_t p_scale_x, real_t p_scale_z) const {
            return sector.position(p_scale_x, p_scale_z);
        }
        _FORCE_INLINE_ Vector3 position(int p_sector_size, int p_lod, int p_num_lods, real_t p_scale_x, real_t p_scale_z) const {
            int lod_shift = p_num_lods - p_lod - 1;
            int cell_size = p_sector_size >> lod_shift;
            return Vector3((sector.cell.x * p_sector_size + cell.cell.x * cell_size) * p_scale_x, 0.0, (sector.cell.z * p_sector_size + cell.cell.z * cell_size) * p_scale_x);
        }

        uint32_t hash() const {
            uint32_t h = hash_murmur3_one_32(uint32_t(sector.key));
            h = hash_murmur3_one_32(uint32_t(cell.key), h);
            return hash_fmix32(h);
        }
    };
    static_assert(sizeof(NodeKey) == 8);

private:
    static constexpr uint16_t HMAP_HOLE_VALUE = UINT16_MAX;
    static constexpr uint16_t HMAP_MAX = HMAP_HOLE_VALUE - 1;

    static const String REGION_FILE_BASE_NAME;
    static const String REGION_FILE_EXTENSION;
    static const String REGION_FILE_FORMAT;

    static const size_t HEADER_SIZE = 32;
    static const size_t MINMAX_OFFSET = HEADER_SIZE;
    static const size_t FILE_HEADER_SIZE = 64;
    static const size_t MAGIC_SIZE = 4;
    static constexpr char unsigned MAGIC_STRING[MAGIC_SIZE] = {'T', 'E', 'R', 'R'};
    static const uint8_t FORMAT_VERSION = 1ui8;

    static const uint8_t FORMAT_PACKED = 0x00;
    static const uint8_t FORMAT_SPARSE = 0x10;
    static const uint8_t FORMAT_PACKAGING_MASK = 0x10;
    static const uint8_t FORMAT_SAVED_LODS_MASK = 0x0F;

    static const uint8_t FORMAT_LITTLE_ENDIAN = 0x11;
    static const uint8_t FORMAT_BIG_ENDIAN = 0x22;

    static constexpr uint8_t REGION_FLAG_HAS_MINMAX = 1 << 0;

    // static constexpr uint32_t CHUNK_FLAG_HAS_MINMAX = 1 << 0;
    // static constexpr uint32_t CHUNK_FLAG_HAS_HEIGHT = 1 << 1;
    // static constexpr uint32_t CHUNK_FLAG_HAS_SPLAT = 1 << 2;
    // static constexpr uint32_t CHUNK_FLAG_HAS_META = 1 << 3;
    // static constexpr uint32_t CHUNK_FLAG_COMPRESSED_MINMAX = 1 << 4;
    // static constexpr uint32_t CHUNK_FLAG_COMPRESSED_HEIGHT = 1 << 5;
    // static constexpr uint32_t CHUNK_FLAG_COMPRESSED_SPLAT = 1 << 6;
    // static constexpr uint32_t CHUNK_FLAG_COMPRESSED_META = 1 << 7;

    static const int MAX_QUEUE_SIZE = 32;
    static const int MAX_RES_QUEUE_SIZE = 128;
    // static const int MAX_POOL_SIZE = 32;

    static constexpr uint32_t DATA_TYPE_MINMAX = 1 << 0;
    static constexpr uint32_t DATA_TYPE_HEIGHT = 1 << 1;
    static constexpr uint32_t DATA_TYPE_SPLAT = 1 << 2;
    static constexpr uint32_t DATA_TYPE_META = 1 << 3;

    static const int MAX_CHUNK_SIZE = 2048;
    static const int MAX_PROCESSED_RESULTS = 10;

    static constexpr float PRIORITY_DISTANCE_FACTOR = 100.0f;
    static constexpr float PRIORITY_DISTANCE_HALF_DECAY = 20.0f;
    static constexpr float PRIORITY_IN_FRUSTUM = 2.0f;
    static constexpr float PRIORITY_MINMAX = 10.0f;
    static constexpr real_t PRIORITY_PREDICTION_DELTA_TIME = 2.0;

    static const int INVALID_TEXTURE_LAYER = -1;
    static const int EXTRA_BUFFER_LAYERS = 8;

    // enum class ChunkState : uint8_t {
    //     Unloaded,
    //     Requested,
    //     IO_InFlight,
    //     Decoding,
    //     ReadyForUpload,
    //     Resident,
    //     Evicting
    // };
    // // TODO: Add HEIGHTMAP_LOADED state. // CPU-side data ready (physics/collision)

    // enum HeightFormat : uint32_t {
    //     Unsigned16,
    //     Signed16,
    //     FloatingPoint16
    // };

    // enum SplatFormat : uint32_t {
    //     RGBA8,
    //     RGB16,
    //     BC3,
    //     BC7
    // };

    // struct alignas(HEADER_SIZE) Header {
    //     char magic[MAGIC_SIZE];
    //     uint8_t endianness;
    //     uint8_t format;
    //     uint16_t version;
    //     uint32_t chunk_size;
    //     uint32_t region_size;
    //     uint32_t minmax_format;
    //     uint32_t height_format;
    //     uint32_t splat_format;
    //     uint32_t directory_offset;
    //     uint64_t data_offset;
    //     // uint8_t reserved[24];

    //     _FORCE_INLINE_ int lods() { return format & FORMAT_SAVED_LODS_MASK; }
    //     _FORCE_INLINE_ bool is_packed() { return (format & FORMAT_PACKAGING_MASK) == FORMAT_PACKED; }
    // };
    // static_assert(sizeof(Header) == HEADER_SIZE);

    struct alignas(HEADER_SIZE) Header {
        uint8_t presence;
        uint8_t version;
        uint8_t minmax_height_format;
        uint8_t splat_meta_format;
        uint8_t minmax_dir_size;
        uint8_t height_dir_size;
        uint8_t splat_dir_size;
        uint8_t meta_dir_size;
        uint64_t height_offset;
        uint64_t splat_offset;
        uint64_t meta_offset;

        _FORCE_INLINE_ bool has_minmax() const { return presence & REGION_FLAG_HAS_MINMAX; };
    };
    static_assert(sizeof(Header) == HEADER_SIZE);

    struct alignas(FILE_HEADER_SIZE) FileHeader {
        char magic[MAGIC_SIZE];
        uint8_t endianness;
        uint8_t format;
        uint8_t u8_reserved1;
        uint8_t u8_reserved2;
        uint32_t chunk_size;
        uint32_t region_size;
        uint64_t u64_reserved1;
        uint64_t u64_reserved2;
        Header header;

        _FORCE_INLINE_ int lods() { return format & FORMAT_SAVED_LODS_MASK; }
    };
    static_assert(sizeof(FileHeader) == FILE_HEADER_SIZE);

    union alignas(FILE_HEADER_SIZE) FileHeaderBytes {
        uint8_t bytes[FILE_HEADER_SIZE];
        FileHeader value;
    };

    // struct ChunkEntry {
    //     uint32_t flags;
    //     uint32_t minmax_size;
    //     uint32_t height_size;
    //     uint32_t splat_size;
    //     uint32_t meta_size;
    //     uint64_t minmax_offset;
    //     uint64_t height_offset;
    //     uint64_t splat_offset;
    //     uint64_t meta_offset;
    // };

    // enum class RegionState {
    //     Reading,
    //     Writing,
    // };

    struct Region {
        Header *header;
        Ref<FileAccess> query_access;
        Ref<FileAccess> data_access;
    };

    struct Tracker {
        void *pointer;
        mutable uint64_t frame;
        mutable bool in_frustum;

        enum class Status : uint8_t {
            UNINITIALIZED,
            LOADING,
            LOADED
        } status;

        Tracker() : frame(0), pointer(nullptr), status(Status::UNINITIALIZED), in_frustum(false) {}
        Tracker(uint64_t p_frame, Status p_status, bool p_in_frustum) : frame(p_frame), pointer(nullptr), status(p_status), in_frustum(p_in_frustum) {}
        _FORCE_INLINE_ bool is_loaded() const { return status == Status::LOADED; }
        _FORCE_INLINE_ bool exists() const { return status != Status::UNINITIALIZED; }

    };

    const Tracker default_tracker;

    struct IORequest {
        NodeKey key;
        Tracker* tracker;
        uint64_t request_id;
        float priority;
        uint16_t data_type;
        uint16_t lod_level;

        IORequest() : tracker(nullptr), request_id(0), priority(0.0f), data_type(0), lod_level(0) {}
        IORequest(NodeKey p_key, Tracker *p_tracker, uint64_t p_request_id, uint16_t p_type, uint16_t p_lod) :
            key(p_key), tracker(p_tracker), request_id(p_request_id), priority(0.0f), data_type(p_type), lod_level(p_lod) {}
    };
    static_assert(sizeof(IORequest) == 32);

    struct RequestCompare {
		_FORCE_INLINE_ bool operator()(const IORequest &p_a, const IORequest &p_b) const {
			return p_a.priority < p_b.priority;
		}
	};

    struct IOResult {
        NodeKey key;
        uint64_t request_id;
        uint16_t data_type;
        uint16_t lod_level;
        void *pointer;

        enum class Status : uint8_t {
            UNKOWN,
            SUCCESS,
            IO_ERROR,            // Disk read failed
            DECOMPRESSION_ERROR, // Corrupt data
            CANCELLED,           // Request was cancelled mid-flight
            OUT_OF_MEMORY        // Pool allocation failed
        } status;

        // Performance tracking.
        uint64_t io_start_time;
        uint64_t io_end_time;
        uint32_t bytes_read_from_disk;  // Compressed size read

        IOResult(const NodeKey &p_key, uint64_t p_request_id, uint16_t p_data_type, uint16_t p_lod):
            key(p_key), request_id(p_request_id), data_type(p_data_type), lod_level(p_lod), pointer(nullptr), status(Status::UNKOWN) {}

        _FORCE_INLINE_ bool is_success() const { return status == Status::SUCCESS; }
        _FORCE_INLINE_ uint64_t latency() const { return io_end_time - io_start_time; }
    };

    struct TextureData {
        PackedByteArray height;
        PackedByteArray splat;
        int layer = INVALID_TEXTURE_LAYER;
    };

    String directory_path;
    uint16_t chunk_size = 32ui16;
    uint16_t region_size = 32ui16;
    bool size_locked = false;
    bool data_locked = false;

    uint16_t sector_size = 0ui16; // In terms of chunks.
    int lods = 0;
    int saved_lods = 5; // log2(32)

    Thread io_thread;
    SafeFlag io_running;

    Vector<IORequest> io_pending;
    SPSCQueue<IORequest> *io_queue = nullptr;
    SPSCQueue<IOResult> *io_result = nullptr;
    uint64_t current_frame = 0;
    uint64_t cancelled_frame = 0;
    uint64_t current_request = 0;
    Vector3 viewer_pos;
    Vector3 viewer_vel;
    Vector3 viewer_forward;
    Vector3 predicted_viewer_pos;
    Vector3 map_scale;

    HashMap<CellKey, Region*> regions;
    Vector<size_t> minmax_lod_offsets;
    BufferPool<hmap_t> *minmax_buffer = nullptr;
    HashMap<CellKey, Tracker> minmax_trackers;
    Mutex minmax_mutex;
    Vector<hmap_t> minmax_read;
    const mutable Tracker* cached_minmax_tracker = nullptr;
    mutable CellKey cached_sector = CellKey(UINT16_MAX, UINT16_MAX);
    SafeFlag minmax_full;
    hmap_t default_height = 0;

    BufferPool<hmap_t> *height_buffer = nullptr;
    Vector<HashMap<NodeKey, Tracker>> textures_trackers;
    Vector<int> unused_texture_layers;
    int num_layers = 0;
    int used_layers = 0;
    int requested_layers = 0;
    RID rd_heightmap_texture;
    Ref<Texture2DArrayRD> heightmap_texture;

    void _clear();
    static void _process_requests(void *p_storage);
    _FORCE_INLINE_ void _add_request(const NodeKey &p_key, Tracker *p_tracker, uint16_t p_data_type, uint16_t p_lod);
    void _submit_requests();
    void _process_results();
    _FORCE_INLINE_ void _load_region_minmax(CellKey p_region_key, hmap_t *p_buffer, size_t p_size);
    void _load_sector_minmax(const NodeKey &p_key, const IORequest &p_request, SPSCQueue<IOResult> *p_queue);
    Region* _create_region(CellKey p_region_key);
    float _calc_request_priority(const Vector3 &p_chunk_pos, bool p_in_frustum);
    _FORCE_INLINE_ bool _is_format_correct(Ref<FileAccess> &p_file) const;

    hmap_t* _free_lru_minmax();
    void _clean_minmax();
    void _cache_minmax(CellKey p_sector) const;

    void _allocate_textures();
    int _next_layer();

protected:
    bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    static void _bind_methods();

public:
    static const int MAX_LOD_LEVELS = 15;
    static const StringName path_changed;

    Error load_headers();
    bool is_sector_loaded(CellKey p_sector) const;
    void load_minmax(CellKey p_sector, bool p_in_frustum);
    void get_minmax(const NodeKey &p_key, int p_lod, hmap_t &r_min, hmap_t &r_max, bool &r_has_data) const;
    void allocate_minmax(int p_sector_chunks, int p_lods, const Vector2i &p_world_regions, const Vector3 &p_map_scale, real_t p_far_view);

    int get_node_texture_layer(const NodeKey &p_key, int p_lod);

    void update_viewer(const Vector3 &p_viewer_pos, const Vector3 &p_viewer_vel, const Vector3 &p_viewer_forward);
    void stop_io();
    void process();

    bool is_directory_set() const;
    void set_directory_path(const String &p_path);
    String get_directory_path() const;
    void set_chunk_size(int p_size);
    int get_chunk_size() const;
    void set_region_size(int p_size);
    int get_region_size() const;
    void set_size_locked(bool p_locked);
    bool is_size_locked() const;
    void set_data_locked(bool p_locked);
    bool is_data_locked() const;
    void set_default_height(hmap_t p_height);

    MapStorage();
    ~MapStorage();
};

} // namespace Terrainer

#endif // TERRAINER_MAP_STORAGE_H