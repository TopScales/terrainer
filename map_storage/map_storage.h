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

private:
    typedef uint16_t hmap_t;

    static constexpr uint16_t HMAP_HOLE_VALUE = UINT16_MAX;

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
    // static const int MAX_POOL_SIZE = 32;

    static constexpr uint32_t DATA_TYPE_MINMAX = 1 << 0;
    static constexpr uint32_t DATA_TYPE_HEIGHT = 1 << 1;
    static constexpr uint32_t DATA_TYPE_SPLAT = 1 << 2;
    static constexpr uint32_t DATA_TYPE_META = 1 << 3;

    static const int MAX_CHUNK_SIZE = 2048;

    static constexpr float PRIORITY_DISTANCE_FACTOR = 100.0f;
    static constexpr float PRIORITY_DISTANCE_HALF_DECAY = 20.0f;
    static constexpr float PRIORITY_IN_FRUSTUM = 2.0f;
    static constexpr float PRIORITY_MINMAX = 10.0f;
    static constexpr float PRIORITY_HEIGHT = 2.0f;
    static constexpr real_t PRIORITY_PREDICTION_DELTA_TIME = 2.0;

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

    struct IORequest {
        Vector2i chunk;
        int64_t frame;
        uint64_t request_id;
        float priority;
        uint16_t data_type;
        uint16_t lod_level;

        IORequest() : frame(0), request_id(0), priority(0.0f), data_type(0), lod_level(0) {}
        IORequest(const Vector2i &p_chunk, int64_t p_frame, uint64_t p_request_id, uint16_t p_type, uint16_t p_lod) :
            chunk(p_chunk), frame(p_frame), request_id(p_request_id), priority(0.0f), data_type(p_type), lod_level(p_lod) {}

        bool operator<(const IORequest &other) const {
            return priority > other.priority;
        }
    };
    static_assert(sizeof(IORequest) == 32);

    struct RequestCompare {
		_FORCE_INLINE_ bool operator()(const IORequest &p_a, const IORequest &p_b) const {
			return p_a.priority > p_b.priority;
		}
	};

    // struct IOResult {
    //     Vector2i chunk;

    //     enum class DataType : uint8_t {
    //         MINMAX,
    //         HEIGHT,
    //         SPLAT,
    //         META,
    //         HEIGHt_AND_SPLAT,
    //         ALL_DATA
    //     } data_type;

    //     // Pointers to the loaded data (nullptr if not loaded).
    //     uint8_t* heightmap;
    //     uint8_t* splatmap;
    //     uint8_t* metadata;

    //     uint32_t heightmap_size;
    //     uint32_t splatmap_size;
    //     uint32_t metadata_size;

    //     enum class Status : uint8_t {
    //         SUCCESS,
    //         IO_ERROR,            // Disk read failed
    //         DECOMPRESSION_ERROR, // Corrupt data
    //         CANCELLED,           // Request was cancelled mid-flight
    //         OUT_OF_MEMORY        // Pool allocation failed
    //     } status;

    //     // Performance tracking.
    //     uint64_t io_start_time;
    //     uint64_t io_end_time;
    //     uint32_t bytes_read_from_disk;  // Compressed size read

    //     uint64_t request_id;

    //     _FORCE_INLINE_ bool is_success() const { return status == Status::SUCCESS; }
    //     _FORCE_INLINE_ bool has_heightmap() const { return heightmap != nullptr; }
    //     _FORCE_INLINE_ bool has_splatmap() const { return splatmap != nullptr; }
    //     uint64_t latency() const { return io_end_time - io_start_time; }
    // };

    String directory_path;
    int chunk_size = 32;
    int region_size = 32;
    bool size_locked = false;
    bool data_locked = false;

    int sector_size = 0; // In terms of chunks.
    int lods = 0;
    int saved_lods = 5; // log2(32)

    Thread io_thread;
    SafeFlag io_running;

    Vector<IORequest> io_pending;
    SPSCQueue<IORequest> *io_queue = nullptr;
    int64_t current_frame = 0;
    int64_t cancelled_frame = 0;
    uint64_t current_request = 0;
    Vector3 viewer_pos;
    Vector3 viewer_vel;
    Vector3 viewer_forward;
    Vector3 predicted_viewer_pos;
    Vector3 map_scale;

    HashMap<Vector2i, Region*> regions;
    Vector<size_t> minmax_lod_offsets;
    HashMap<Vector2i, hmap_t*> sector_minmax;
    BufferPool<hmap_t> *minmax_buffer = nullptr;
    Vector<hmap_t> minmax_read;
    hmap_t default_height = 0;

    // SPSCQueue<IOResult> *result_queue = nullptr;
    // ChunkedPool *height_pool = nullptr;
    // ChunkedPool *splat_pool = nullptr;

    // Vector2i chunk;
    //     int64_t frame;
    //     uint64_t request_id;
    //     float priority;
    //     uint16_t data_type;
    //     uint16_t lod_level;

    void _clear();
    static void _process_requests(void *p_storage);
    _FORCE_INLINE_ void _add_request(const Vector2i &p_chunk, uint16_t p_data_type, uint16_t p_lod);
    void _submit_requests();
    _FORCE_INLINE_ void _load_region_minmax(const Vector2i &p_region_key, hmap_t *p_buffer, size_t p_size);
    void _load_sector_minmax(const Vector2i &p_sector);
    void _create_region(const Vector2i &p_region_key, Region *r_region);
    float _calc_request_priority(const Vector3 &p_chunk_pos, bool p_in_frustum);
    _FORCE_INLINE_ bool _is_format_correct(Ref<FileAccess> &p_file) const;

protected:
    // void _notification(int p_what);
    bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    static void _bind_methods();

public:
    static const int MAX_LOD_LEVELS = 15;
    static const StringName path_changed;

    Error load_headers();
    hmap_t* get_sector_minmax_ptr(const Vector2i &p_sector);
    void get_minmax(uint16_t p_x, uint16_t p_z, int p_lod, uint16_t &r_min, uint16_t &r_max) const;
    void allocate_minmax(int p_sector_chunks, int p_lods, const Vector2i &p_world_regions, const Vector3 &p_map_scale, real_t p_far_view);
    void update_viewer(const Vector3 &p_viewer_pos, const Vector3 &p_viewer_vel, const Vector3 &p_viewer_forward);
    void stop_io();

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