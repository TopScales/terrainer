/**
 * terrain.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_TERRAIN_H
#define TERRAINER_TERRAIN_H

#include "lod_quad_tree.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/camera_3d.h"
#include "utils.h"

class TTerrain : public Node3D {
    GDCLASS(TTerrain, Node3D);

private:
    static const real_t UPDATE_TOLERANCE_FACTOR;
    static const int MAX_CHUNK_SIZE = 2048;
    static const int MIN_QUAD_TREE_LEAD_SIZE = 2;

    int chunk_size = 32;
    Vector2i chunk_world_offset;
    bool centered = false;
    real_t quad_size = 1.0;
    bool chunk_manual_update = false;
    Vector2i chunk_active;

    int mesh_render_resolution_multiplier = 1;
    int mesh_lod_detail_levels = 4;

    TTerrainData data;
    Ref<TLODQuadTree> quad_tree;
    Transform3D last_transform;
    Transform3D viewer_transform;
    bool dirty = false;
    bool update_view = false;
    real_t update_distance_tolerance_squared = (chunk_size * quad_size * UPDATE_TOLERANCE_FACTOR) * (chunk_size * quad_size * UPDATE_TOLERANCE_FACTOR);
    Camera3D *camera;

    void _enter_world();

    void _setup();
    void _set_viewport_camera();
    void _update_viewer();
    void _update_view();
    void _check_mesh_render_resolution();

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    void set_camera(Camera3D *p_camera);

    void set_chunk_size(int p_size);
    int get_chunk_size() const;
    void set_chunk_world_offset(Vector2i p_offset);
    Vector2i get_chunk_world_offset() const;
    void set_centered(bool p_centered);
    bool is_centered() const;
    void set_unit_size(real_t p_size);
    real_t get_unit_size() const;
    void set_chunk_manual_update(bool p_manual);
    bool is_chunk_manual_update() const;
    void set_chunk_active(Vector2i p_active);
    Vector2i get_chunk_active() const;

    void set_mesh_quad_tree_leaf_size(int p_size);
    int get_mesh_quad_tree_leaf_size() const;
    void set_mesh_render_resolution_multiplier(int p_multiplier);
    int get_mesh_render_resolution_multiplier() const;
    void set_mesh_lod_level_count(int p_count);
    int get_mesh_lod_level_count() const;
    void set_mesh_lod_distance_ratio(real_t p_ratio);
    real_t get_mesh_lod_distance_ratio() const;
    void set_mesh_lod_detail_levels(int p_levels);
    int get_mesh_lod_detail_levels() const;

	TTerrain();
    ~TTerrain();
};

#endif // TERRAINER_TERRAIN_H