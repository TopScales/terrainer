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

#include "chunked_pool.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
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
    static const String REGION_FILE_BASE_NAME;
    static const String REGION_FILE_EXTENSION;
    static const String REGION_FILE_FORMAT;

    static const int HEADER_SIZE = 64;
    static const int MAGIC_SIZE = 4;
    static constexpr char unsigned MAGIC_STRING[MAGIC_SIZE] = {'T', 'E', 'R', 'R'};
    static const uint16_t FORMAT_VERSION = 1;

    static const uint8_t FORMAT_PACKED = 0x00;
    static const uint8_t FORMAT_SPARSE = 0x10;
    static const uint8_t FORMAT_PACKAGING_MASK = 0x10;
    static const uint8_t FORMAT_SAVED_LODS_MASK = 0x0F;

    static const uint8_t FORMAT_LITTLE_ENDIAN = 0x11;
    static const uint8_t FORMAT_BIG_ENDIAN = 0x22;

    static constexpr uint32_t CHUNK_FLAG_HAS_MINMAX = 1 << 0;
    static constexpr uint32_t CHUNK_FLAG_HAS_HEIGHT = 1 << 1;
    static constexpr uint32_t CHUNK_FLAG_HAS_SPLAT = 1 << 2;
    static constexpr uint32_t CHUNK_FLAG_HAS_META = 1 << 3;
    static constexpr uint32_t CHUNK_FLAG_COMPRESSED_MINMAX = 1 << 4;
    static constexpr uint32_t CHUNK_FLAG_COMPRESSED_HEIGHT = 1 << 5;
    static constexpr uint32_t CHUNK_FLAG_COMPRESSED_SPLAT = 1 << 6;
    static constexpr uint32_t CHUNK_FLAG_COMPRESSED_META = 1 << 7;

    static const int MAX_QUEUE_SIZE = 32;
    static const int MAX_POOL_SIZE = 32;

    static constexpr uint32_t DATA_TYPE_MINMAX = 1 << 0;
    static constexpr uint32_t DATA_TYPE_HEIGHT = 1 << 1;
    static constexpr uint32_t DATA_TYPE_SPLAT = 1 << 2;
    static constexpr uint32_t DATA_TYPE_META = 1 << 3;

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

    struct alignas(HEADER_SIZE) Header {
        char magic[MAGIC_SIZE];
        uint8_t endianness;
        uint8_t format;
        uint16_t version;
        uint32_t chunk_size;
        uint32_t region_size;
        uint32_t minmax_format;
        uint32_t height_format;
        uint32_t splat_format;
        uint32_t directory_offset;
        uint64_t data_offset;
        // uint8_t reserved[24];

        _FORCE_INLINE_ int lods() { return format & FORMAT_SAVED_LODS_MASK; }
        _FORCE_INLINE_ bool is_packed() { return (format & FORMAT_PACKAGING_MASK) == FORMAT_PACKED; }
    };
    static_assert(sizeof(Header) == HEADER_SIZE);

    union HeaderBytes {
        uint8_t bytes[HEADER_SIZE];
        Header value;
    };

    struct ChunkEntry {
        uint32_t flags;
        uint32_t minmax_size;
        uint32_t height_size;
        uint32_t splat_size;
        uint32_t meta_size;
        uint64_t minmax_offset;
        uint64_t height_offset;
        uint64_t splat_offset;
        uint64_t meta_offset;
    };


    struct Region
    {
        Header *header;
        Ref<FileAccess> query_access;
        Ref<FileAccess> data_access;
    };

    struct IORequest {
        Vector2i chunk;
        uint64_t frame_id;
        uint64_t request_id;
        uint32_t data_type;
        float priority;
        uint8_t lod_level;
    };

    struct IOResult {
        Vector2i chunk;

        enum class DataType : uint8_t {
            MINMAX,
            HEIGHT,
            SPLAT,
            META,
            HEIGHt_AND_SPLAT,
            ALL_DATA
        } data_type;

        // Pointers to the loaded data (nullptr if not loaded).
        uint8_t* heightmap;
        uint8_t* splatmap;
        uint8_t* metadata;

        uint32_t heightmap_size;
        uint32_t splatmap_size;
        uint32_t metadata_size;

        enum class Status : uint8_t {
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

        uint64_t request_id;

        _FORCE_INLINE_ bool is_success() const { return status == Status::SUCCESS; }
        _FORCE_INLINE_ bool has_heightmap() const { return heightmap != nullptr; }
        _FORCE_INLINE_ bool has_splatmap() const { return splatmap != nullptr; }
        uint64_t latency() const { return io_end_time - io_start_time; }
    };

    String directory_path;
    int chunk_size = 32;
    int region_size = 32;
    bool size_locked = false;
    bool data_locked = false;

    bool pools_ready = false;
    int sector_size = 0;

    HashMap<Vector2i, Region*> regions;
    SPSCQueue<IORequest> *io_queue = nullptr;
    SPSCQueue<IOResult> *result_queue = nullptr;
    ChunkedPool *height_pool = nullptr;
    ChunkedPool *splat_pool = nullptr;

    _FORCE_INLINE_ bool _is_format_correct(Ref<FileAccess> &p_file) const;

protected:
    // void _notification(int p_what);
    bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    static void _bind_methods();

public:
    static const StringName path_changed;

    Error load_headers();
    void clear();

    bool is_directory_set() const;
    void set_directory_path(String p_path);
    String get_directory_path() const;
    void set_chunk_size(int p_size);
    int get_chunk_size() const;
    void set_region_size(int p_size);
    int get_region_size() const;
    void set_size_locked(bool p_locked);
    bool is_size_locked() const;
    void set_data_locked(bool p_locked);
    bool is_data_locked() const;
    void set_sector_size(int p_size);

    MapStorage();
    ~MapStorage();
};

} // namespace Terrainer

#endif // TERRAINER_MAP_STORAGE_H