/**
 * map_storage.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#include "map_storage2.h"

#include "core/io/dir_access.h"
#include "../utils/compat_marshalls.h"
#include "../utils/math.h"

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
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "Can't iterate over files in MapStorage directory.");
    String file_name = dir->get_next();
    Vector2i block_shift = world_info.world_blocks / 2;

    while (!file_name.is_empty()) {
        if (!dir->current_is_dir() && file_name.begins_with(base_name) && file_name.get_extension() == DATA_FILE_EXT) {
            String remaining = file_name.get_basename().right(-base_name.length());
            PackedStringArray parts = remaining.split("_", false);

            if (parts.size() == 2) {
                int x = parts[0].to_int() + block_shift.x;
                int z = parts[1].to_int() + block_shift.y;
                String block_file = directory.path_join(file_name);
                Ref<FileAccess> file = FileAccess::open(block_file, FileAccess::READ_WRITE, &error);
                ERR_CONTINUE_EDMSG(error != OK, vformat("Can`t open stream block file %s.", block_file));
                uint8_t version = file->get_8();
                ERR_CONTINUE_EDMSG(version > FORMAT_VERSION, vformat("Wrong format version in block file %s.", file_name));
                uint8_t shifts = file->get_8();
                int csz = 1 << (shifts & 0x0F);
                ERR_CONTINUE_EDMSG(csz != world_info.chunk_size, vformat("Incorrect chunk size in block file %s.", file_name));
                int bsz = 1 << ((shifts & 0xF0) >> 4);
                ERR_CONTINUE_EDMSG(bsz != world_info.block_size, vformat("Incorrect block size in block file %s.", file_name));
                uint16_t format = file->get_16();
                ERR_CONTINUE_EDMSG(Block::format_to_segment_size(format) != segment_size, vformat("Incorrect segment size in block file %s.", file_name));
                uint16_t splat_map_addr = file->get_16();
                uint16_t meta_addr = file->get_16();
                Vector2i pos = Vector2i(x, z);
                Block *block = memnew(Block(pos, file, version, format, splat_map_addr, meta_addr));
                blocks[pos] = block;
            }
        }

        file_name = dir->get_next();
    }

    dir->list_dir_end();
    return OK;
}

void TMapStorage::setup(const TTerrainInfo &p_info) {
    minmax_map.setup(p_info, world_info);
    lod_levels = p_info.lod_levels;
    num_layer_blocks = (1 << (lod_levels - 1)) / world_info.block_size;

    for (int iz = 0; iz < world_info.world_blocks.y; ++iz) {
        for (int ix = 0; ix < world_info.world_blocks.x; ++ix) {
            const Vector2i block_index = Vector2i(ix, iz);
            Block **block_ptr = blocks.getptr(block_index);

            if (block_ptr && (*block_ptr)->has_heightmap()) {
                const Block *block = *block_ptr;
                const int minmax_lods = block->get_max_minmax_lod();
                ERR_CONTINUE_EDMSG(minmax_lods != minmax_map.get_saved_lods(), vformat("Wrong minmax LODs in map block (%d, %d).", ix, iz));
                minmax_map.load_block(block_index, block->file, world_info);
            } else {
                minmax_map.fill_block(block_index, 0, world_info);
            }
        }
    }

    minmax_map.generate_remaining_lods(world_info);


    if (rd_heightmap_texture.is_valid()) {
        RenderingServer::get_singleton()->get_rendering_device()->free_rid(rd_heightmap_texture);
    }
}

bool TMapStorage::get_node_texture_layer(const Vector2i &p_node_pos, int &r_layer) {
    TextureData **pdata = texture_data_map.getptr(p_node_pos);

    if (pdata) {
        TextureData *data = *pdata;
        data->frame = frame;
        r_layer = data->layer;
        return (*pdata)->loaded;
    } else {
        TextureData *data = memnew(TextureData);
        data->frame = frame;

        if (unused_layers.is_empty()) {
            data->layer = used_layers;
        } else {
            int new_size = unused_layers.size() - 1;
            data->layer = unused_layers[new_size];
            unused_layers.resize(new_size);
        }

        used_layers++;
        r_layer = data->layer;
        tex_data_mutex.lock();
        texture_data_map[p_node_pos] = data;
        tex_data_mutex.unlock();
        _request_texture_layer(p_node_pos, data);
        return false;
    }
}

void TMapStorage::load_from_buffer(const PackedByteArray &p_heightmap) {
    clear();
    keep_data = true;
    int block_size = world_info.chunk_size * world_info.block_size;
    int src_buffer_size = block_size * block_size * world_info.world_blocks.x * world_info.world_blocks.y;
    ERR_FAIL_COND_EDMSG(p_heightmap.size() != src_buffer_size, "Incorrect heightmap size.");
    uint16_t format = Block::get_format(max_saved_lods, minmax_map.get_saved_lods(), segment_size, true, false, false);
    int hmap_size = 4 * HEIGHTMAP_DATA_BYTES * block_size * block_size * (1 - 1 / Math::pow(4.0, max_saved_lods)) / 3;

    for (int ibz = 0; ibz < world_info.world_blocks.y; ++ibz) {
        for (int ibx = 0; ibx < world_info.world_blocks.x; ++ibx) {
            const Vector2i block_index = Vector2i(ibx, ibz);
            Block *block = memnew(Block(block_index, FORMAT_VERSION, format, 0, 0));
            block->heightmap.resize(hmap_size);
            blocks[block_index] = block;
            int index = 0;
            uint8_t *hmap = block->heightmap.ptrw();

            for (int iz = 0; iz < block_size; ++iz) {
                for (int ix = 0; ix < block_size; ++ix) {
                    int src_idx = ix + ibx * block_size + (iz + ibz * block_size) * block_size * world_info.world_blocks.x;
                    hmap[HEIGHTMAP_DATA_BYTES * index] = p_heightmap[src_idx];
                    index++;
                }
            }

            int src_offset = 0;
            int offset = HEIGHTMAP_DATA_BYTES * block_size * block_size;
            int lod_buffer_size = offset >> 2;
            block_size >>= 1;
            const int src_inc = 2 * HEIGHTMAP_DATA_BYTES;

            for (int ilod = 1; ilod < max_saved_lods; ++ilod) {
                for (int iz = 0; iz < block_size; ++iz) {
                    for (int ix = 0; ix < block_size; ++ix) {
                        const int src_idx = src_offset + HEIGHTMAP_DATA_BYTES * (2 * ix + 4 * iz * block_size);
                        uint32_t h00 = hmap[src_idx];
                        uint32_t h10 = hmap[src_idx + src_inc];
                        uint32_t h01 = hmap[src_idx + 2 * src_inc * block_size];
                        uint32_t h11 = hmap[src_idx + src_inc * (1 + 2 * block_size)];
                        const int idx = HEIGHTMAP_DATA_BYTES * (ix + iz * block_size);
                        hmap[idx] = (h00 + h10 + h01 + h11) >> 2;
                    }
                }

                src_offset = offset;
                offset += lod_buffer_size;
                lod_buffer_size >>= 2;
                block_size >>= 1;
            }
        }
    }
}

void TMapStorage::arrange_layers() {
    if (used_layers > num_layers) {
        if (rd_heightmap_texture.is_valid()) {
            RenderingServer::get_singleton()->get_rendering_device()->free_rid(rd_heightmap_texture);
        }

        int size = num_layer_blocks * world_info.block_size * world_info.chunk_size + 1;
        num_layers = used_layers + EXTRA_BUFFER_LAYERS;
        _create_textures(num_layers, size, size, lod_levels);
        unused_layers.resize(EXTRA_BUFFER_LAYERS);

        for (int i = 0; i < EXTRA_BUFFER_LAYERS; ++i) {
            unused_layers.set(i, used_layers + i);
        }
    }

    Vector<Vector2i> to_delete;

    for (KeyValue<Vector2i, TextureData*> &kv : texture_data_map) {
        TextureData *data = kv.value;

        if (frame - data->frame > UNLOAD_FRAME_TOLERANCE) {
            if (!keep_data) {
                for (Block *block : data->blocks) {
                    Vector2i block_index = block->position;
                    block->clear();
                    memdelete(block);
                    blocks.erase(block_index);
                }
            }

            unused_layers.push_back(data->layer);
            to_delete.push_back(kv.key);
            memdelete(data);
            used_layers--;
        }
    }

    tex_data_mutex.lock();

    for (int i = 0; i < to_delete.size(); ++i) {
        texture_data_map.erase(to_delete[i]);
    }

    tex_data_mutex.unlock();
    frame++;
}

void TMapStorage::clear() {
    for (KeyValue<Vector2i, Block*> &kv : blocks) {
        Block *block = kv.value;
        block->clear();
        memdelete(block);
    }

    for (KeyValue<Vector2i, TextureData*> &kv : texture_data_map) {
        TextureData *data = kv.value;
        memdelete(data);
    }

    blocks.clear();
    texture_data_map.clear();
    minmax_map.clear();
    used_layers = 0;
}

void TMapStorage::set_chunk_size(int p_size) {
    ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain chunk size must be greater than zero.");
    ERR_FAIL_COND_EDMSG(p_size * world_info.block_size > MAX_BLOCK_CELLS, "Number of terrain block cells exceeded.");
    const int size = t_round_po2(p_size, world_info.chunk_size);

    if (size != world_info.chunk_size) {
        world_info.chunk_size = size;
        _select_segment_size();
        _set_saved_lods();
        emit_changed();
    }
}

int TMapStorage::get_chunk_size() const {
    return world_info.chunk_size;
}

void TMapStorage::set_block_size(int p_size) {
    ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain block size must be greater than zero.");
    ERR_FAIL_COND_EDMSG(p_size * world_info.chunk_size > MAX_BLOCK_CELLS, "Number of block cells exceeded.");
    const int size = t_round_po2(p_size, world_info.block_size);

    if (size != world_info.block_size) {
        world_info.block_size = size;
        _select_segment_size();
        _set_saved_lods();
        emit_changed();
    }
}

int TMapStorage::get_block_size() const {
    return world_info.block_size;
}

void TMapStorage::set_world_blocks(const Vector2i &p_blocks) {
    Vector2i wblocks = p_blocks;

    if (wblocks.x <= 1) {
        wblocks.x = 1;
    } else if (wblocks.x % 2 == 1) {
        wblocks.x = p_blocks.x > world_info.world_blocks.x ? p_blocks.x + 1 : p_blocks.x - 1;
    }

    if (wblocks.y <= 1) {
        wblocks.y = 1;
    } else if (wblocks.y % 2 == 1) {
        wblocks.y = p_blocks.y > world_info.world_blocks.y ? p_blocks.y + 1 : p_blocks.y - 1;
    }

	world_info.world_blocks = wblocks;
	emit_changed();
}

Vector2i TMapStorage::get_world_blocks() const {
	return world_info.world_blocks;
}

void TMapStorage::set_max_saved_lods(int p_max_lods) {
    if (p_max_lods == 0) {
        max_saved_lods = max_saved_lods > 0 ? -1 : 1;
    } else {
        max_saved_lods = p_max_lods;
    }

    _set_saved_lods();
}

int TMapStorage::get_max_saved_lods() const {
    return max_saved_lods;
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

Ref<Texture2DArrayRD> TMapStorage::get_heightmap_texture() const {
    return heightmap_texture;
}

TWorldInfo *TMapStorage::get_world_info() {
    return &world_info;
}

const TMinmaxMap *TMapStorage::get_minmax_map() const {
    return &minmax_map;
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
    ClassDB::bind_method(D_METHOD("set_world_blocks", "block"), &TMapStorage::set_world_blocks);
	ClassDB::bind_method(D_METHOD("get_world_blocks"), &TMapStorage::get_world_blocks);
    ClassDB::bind_method(D_METHOD("set_max_saved_lods", "max_lods"), &TMapStorage::set_max_saved_lods);
	ClassDB::bind_method(D_METHOD("get_max_saved_lods"), &TMapStorage::get_max_saved_lods);
    ClassDB::bind_method(D_METHOD("set_directory_use_custom", "use_custom"), &TMapStorage::set_directory_use_custom);
	ClassDB::bind_method(D_METHOD("is_directory_use_custom"), &TMapStorage::is_directory_use_custom);
    ClassDB::bind_method(D_METHOD("set_directory_path", "path"), &TMapStorage::set_directory_path);
	ClassDB::bind_method(D_METHOD("get_directory_path"), &TMapStorage::get_directory_path);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size", PROPERTY_HINT_RANGE, "1,256,1"), "set_chunk_size", "get_chunk_size");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "block_size", PROPERTY_HINT_RANGE, "1,256,1"), "set_block_size", "get_block_size");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "world_blocks"), "set_world_blocks", "get_world_blocks");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_saved_lods", PROPERTY_HINT_RANGE, "-1,15,1"), "set_max_saved_lods", "get_max_saved_lods");
}

void TMapStorage::_select_segment_size() {
    int total_size = world_info.chunk_size * world_info.block_size;

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

void TMapStorage::_set_saved_lods() {
    saved_lods = t_log2i(world_info.chunk_size * world_info.block_size);
    minmax_map.set_saved_lods(t_log2i(world_info.chunk_size));

    if (max_saved_lods != -1) {
        saved_lods = MIN(saved_lods, max_saved_lods);
        minmax_map.set_saved_lods(MIN(minmax_map.get_saved_lods(), max_saved_lods));
    }
}

void TMapStorage::_request_texture_layer(const Vector2i &p_node_pos, TextureData *p_texture_data) {
    WorkerThreadPool *wp = WorkerThreadPool::get_singleton();
    int total_blocks = num_layer_blocks * num_layer_blocks;
    p_texture_data->jobs = total_blocks;
    p_texture_data->blocks.resize(total_blocks);

    for (int ibz = 0; ibz < num_layer_blocks; ++ibz) {
        for (int ibx = 0; ibx < num_layer_blocks; ++ibx) {
            const Vector2i block_index = Vector2i(p_node_pos.x * num_layer_blocks + ibx, p_node_pos.y * num_layer_blocks + ibz);
            Block *block = blocks.get(block_index);
            p_texture_data->blocks.set(ibx + ibz * num_layer_blocks, block);
        }
    }

    Callable action = callable_mp(this, &TMapStorage::_load_block_data).bind(p_node_pos);
    WorkerThreadPool::TaskID id = wp->add_group_task(action, total_blocks, -1, false, "Map Storage load block group task");
    tasks_mutex.lock();
    tasks.insert(id);
    tasks_mutex.unlock();
}

void TMapStorage::_load_block_data(const Vector2i &p_nopde_pos, int p_job_id) {
    tex_data_mutex.lock();
    TextureData *tex_data = texture_data_map.get(p_nopde_pos);
    tex_data_mutex.unlock();
    Block *block = tex_data->blocks[p_job_id];

    if (block->heightmap.is_empty()) {
        // TODO: Read file.
    }

    tex_data->job_mutex.lock();
    tex_data->jobs--;
    bool last = tex_data->jobs == 0;
    tex_data->job_mutex.unlock();

    if (last) {
        int index = 0;
        PackedByteArray hm_buffer;
        const int block_size = world_info.block_size * world_info.chunk_size;
        int dst_size = num_layer_blocks * block_size; // FIXME: Should add 1??
        int buffer_size = 4 * dst_size * dst_size * (1 - 1 / Math::pow(4.0, lod_levels)) / 3 +
            4 * dst_size * (1 - 1 / Math::pow(2.0, lod_levels)) + lod_levels;
        hm_buffer.resize(buffer_size);
        uint8_t *hm_ptr = hm_buffer.ptrw();

        for (int iz = 0; iz < num_layer_blocks; ++iz) {
            for (int ix = 0; ix < num_layer_blocks; ++ix) {
                const uint8_t *src = tex_data->blocks[index]->heightmap.ptr();
                int dst_lod_buffer_size = dst_size * dst_size;
                int bsize = block_size;
                int src_lod_buffer_size = block_size * block_size;
                int dst_offset = 0;
                int offset = 0;
                index++;

                for (int ilod = 0; ilod < max_saved_lods; ++ilod) {
                    const int block_offset = (ix + iz * bsize) * bsize;

                    for (int ibz = 0; ibz < bsize; ++ibz) {
                        for (int ibx = 0; ibx < bsize; ++ibx) {
                            const int src_idx = offset + HEIGHTMAP_DATA_BYTES * (ibx + ibz * bsize);
                            const uint16_t h = decode_uint16(src + src_idx);
                            const int dst_idx = HEIGHTMAP_DATA_BYTES * (dst_offset + block_offset + ibx + ibz * dst_size);
                            encode_uint16(h, hm_ptr + dst_idx);
                        }
                    }

                    bsize >>= 1;
                    offset += src_lod_buffer_size;
                    src_lod_buffer_size >>= 2;
                    dst_offset += dst_lod_buffer_size;
                    dst_lod_buffer_size >>= 2;
                }
            }
        }

        dst_size >>= max_saved_lods;

        for (int ilod = max_saved_lods; ilod < lod_levels; ++ilod) {
            for (int iz = 0; iz < dst_size; ++iz) {
                for (int ix = 0; ix < dst_size; ++ix) {
                    const int src_idx = 0;
                }
            }
        }
        // for (int ilod = 1; ilod < max_saved_lods; ++ilod) {
        //         for (int iz = 0; iz < block_size; ++iz) {
        //             for (int ix = 0; ix < block_size; ++ix) {
        //                 const int src_idx = src_offset + HEIGHTMAP_DATA_BYTES * (2 * ix + 4 * iz * block_size);
        //                 uint32_t h00 = hmap[src_idx];
        //                 uint32_t h10 = hmap[src_idx + src_inc];
        //                 uint32_t h01 = hmap[src_idx + 2 * src_inc * block_size];
        //                 uint32_t h11 = hmap[src_idx + src_inc * (1 + 2 * block_size)];
        //                 const int idx = HEIGHTMAP_DATA_BYTES * (ix + iz * block_size);
        //                 hmap[idx] = (h00 + h10 + h01 + h11) >> 2;
        //             }
        //         }

        //         src_offset = offset;
        //         offset += lod_buffer_size;
        //         lod_buffer_size >>= 2;
        //         block_size >>= 1;
        //     }

        RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
        rd->texture_update(rd_heightmap_texture, tex_data->layer, hm_buffer);
    }
}

void TMapStorage::_create_textures(int p_layers, int p_width, int p_height, int p_mipmaps) {
    RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
    RenderingDevice::TextureFormat tex_format;
    tex_format.array_layers = p_layers;
    tex_format.format = RenderingDevice::DATA_FORMAT_R8G8_UNORM;
    tex_format.width = p_width;
    tex_format.height = p_height;
    tex_format.mipmaps = p_mipmaps;
    tex_format.texture_type = RenderingDevice::TEXTURE_TYPE_2D_ARRAY;
    tex_format.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
    RenderingDevice::TextureView tex_view;
    rd_heightmap_texture = rd->texture_create(tex_format, tex_view);
    heightmap_texture->set_texture_rd_rid(rd_heightmap_texture);
}

TMapStorage::TMapStorage() {
    heightmap_texture.instantiate();
}

TMapStorage::~TMapStorage() {
    clear();
    RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();

    if (rd_heightmap_texture.is_valid()) {
        rd->free_rid(rd_heightmap_texture);
    }
}