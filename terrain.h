/**
 * terrain.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_TERRAIN_H
#define TERRAINER_TERRAIN_H

#include "lod_quad_tree.h"
#include "map_storage/map_storage.h"
// #include "terrain_info.h"

#ifdef TERRAINER_MODULE
#include "scene/3d/node_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/resources/mesh.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#endif // TERRAINER_GDEXTENSION

namespace Terrainer {

class Terrain : public Node3D {
    GDCLASS(Terrain, Node3D);

private:
    static const real_t UPDATE_TOLERANCE_FACTOR;
//     static const int MAX_CHUNK_SIZE = 2048;

//     static const int DIRTY_DATA = 1 << 1;
//     static const int DIRTY_CHUNKS = 1 << 2;

//     static const real_t DEBUG_AABB_LOD0_MARGIN;
//     static const real_t DEBUG_AABB_MARGIN_LOD_SCALE_FACTOR;

//     Ref<ShaderMaterial> material;
    Ref<MapStorage> storage;
    Vector3 map_scale = Vector3(1.0, 1.0, 1.0);
    Vector2i world_regions = Vector2i(4, 4);
    int lod_detailed_chunks_radius = 4;

    bool mesh_valid = false;
    RID mesh;
    RID mm_chunks;
    RID mm_instance;

    LODQuadTree quad_tree;
    Transform3D last_transform;
    bool inside_world = false;
    Camera3D *camera = nullptr;
    real_t far_view = 0.0;
    bool use_viewport_camera = true;
    Error storage_status = ERR_CANT_ACQUIRE_RESOURCE;
    Transform3D viewer_transform;
    bool dirty = false;

    real_t update_distance_tolerance_squared = 1.0;


//     struct DebugAABB {
//         RID shader;
//         RID material;
//         RID mesh;
//         RID multimesh;
//         RID instance;
//         PackedColorArray lod_colors;
//     } debug_aabb;

//     bool debug_nodes_aabb_enabled = false;

    void _enter_world();
    void _exit_world();
    void _update_visibility();
    void _update_transform();
    void _update_viewer();
    void _update_chunks();

    void _set_viewport_camera();
    void _create_mesh();
    void _set_lod_levels();
    void _storage_changed();
    void _storage_path_changed();
    _FORCE_INLINE_ void _set_update_distance_tolerance_squared();

//     _FORCE_INLINE_ void _configure_chunk_mesh(RenderingServer *p_rs, const TLODQuadTree::QTNode *p_node, int p_instance_index);

//     void _debug_nodes_aabb_create();
//     void _debug_nodes_aabb_free();
//     void _debug_nodes_aabb_draw() const;
//     void _debug_nodes_aabb_set_colors();

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    void set_camera(Camera3D *p_camera);

    void set_storage(const Ref<MapStorage> &p_storage);
    Ref<MapStorage> get_storage() const;
    void set_map_scale(const Vector3 &p_scale);
    Vector3 get_map_scale() const;
    void set_world_regions(const Vector2i &p_regions);
    Vector2i get_world_regions() const;
//     void set_material(const Ref<ShaderMaterial> &p_material);
//     Ref<ShaderMaterial> get_material() const;
    void set_lod_detailed_chunks_radius(int p_radius);
    int get_lod_detailed_chunks_radius() const;
    void set_lod_distance_ratio(real_t p_ratio);
    real_t get_lod_distance_ratio() const;

    int info_get_lod_levels() const;
    int info_get_lod_nodes_count(int p_level) const;
    int info_get_selected_nodes_count() const;

//     void set_debug_nodes_aabb_enabled(bool p_enabled);
//     bool is_debug_nodes_aabb_enabled() const;

	Terrain();
    ~Terrain();
};

} // namespace Terrainer

#endif // TERRAINER_TERRAIN_H