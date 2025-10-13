/**
 * terrain.cpp
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#include "terrain.h"

#include "utils/macros.h"
#include "utils/math.h"

#ifdef TERRAINER_MODULE
#include "scene/main/viewport.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#endif // TERRAINER_GDEXTENSION

const real_t TTerrain::UPDATE_TOLERANCE_FACTOR = 0.05;
const real_t TTerrain::DEBUG_AABB_LOD0_MARGIN = 2.0;
const real_t TTerrain::DEBUG_AABB_MARGIN_LOD_SCALE_FACTOR = 0.5;

void TTerrain::set_camera(Camera3D *p_camera) {
	if (p_camera != camera) {
		camera = p_camera;
		far_view = -1.0;
	}

	use_viewport_camera = camera == nullptr;
}

void TTerrain::set_chunk_size(int p_size) {
	ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain chunk size must be greater than zero.");
	ERR_FAIL_COND_EDMSG(p_size > MAX_CHUNK_SIZE, vformat("Terrain chunk size must be at most %d.", MAX_CHUNK_SIZE));
	const int size = t_round_po2(p_size, info.chunk_size);

	if (size != info.chunk_size) {
		info.chunk_size = size;
		_set_update_distance_tolerance_squared();

		if (inside_world) {
			_create_mesh();
		} else {
			mesh_valid = false;
		}
	}

	if (material.is_valid()) {
		material->set_shader_parameter("grid_const", Vector2(0.5 * (real_t)info.chunk_size, 2.0 / (real_t)info.chunk_size));
	}
}

int TTerrain::get_chunk_size() const {
	return info.chunk_size;
}

void TTerrain::set_map_scale(const Vector3 &p_scale) {
	ERR_FAIL_COND_EDMSG(p_scale.x <= 0.0 || p_scale.y <= 0.0 || p_scale.z <= 0.0, "Scale must be positive.");
	info.map_scale = p_scale;
	_set_update_distance_tolerance_squared();
	dirty = true;
}

Vector3 TTerrain::get_map_scale() const {
	return info.map_scale;
}

void TTerrain::set_block_size(int p_size) {
	ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain block size must be greater than zero.");
	info.block_size = t_round_po2(p_size, info.block_size);
}

int TTerrain::get_block_size() const {
	return info.block_size;
}

void TTerrain::set_world_blocks(const Vector2i &p_blocks) {
	info.world_blocks = p_blocks;
	dirty = true;

	if (info.world_blocks.x < 1) {
		info.world_blocks.x = 1;
	}

	if (info.world_blocks.y < 1) {
		info.world_blocks.y = 1;
	}
}

Vector2i TTerrain::get_world_blocks() const {
	return info.world_blocks;
}

void TTerrain::set_material(const Ref<ShaderMaterial> &p_material) {
	material = p_material;

	if (mesh_valid) {
		RenderingServer::get_singleton()->mesh_surface_set_material(mesh, 0, material->get_rid());
	}

	if (material.is_valid()) {
		material->set_shader_parameter("grid_const", Vector2(0.5 * (real_t)info.chunk_size, 2.0 / (real_t)info.chunk_size));
	}
}

Ref<ShaderMaterial> TTerrain::get_material() const {
	return material;
}

void TTerrain::set_lod_detailed_chunks_radius(int p_radius) {
	lod_detailed_chunks_radius = p_radius;
	_set_lod_levels();
}

int TTerrain::get_lod_detailed_chunks_radius() const {
	return lod_detailed_chunks_radius;
}

void TTerrain::set_lod_distance_ratio(real_t p_ratio) {
	ERR_FAIL_COND_EDMSG(p_ratio < 1.0, "LOD level distance ratio must be at least 1.");
	info.lod_distance_ratio = p_ratio;
	_set_lod_levels();
}

real_t TTerrain::get_lod_distance_ratio() const {
	return info.lod_distance_ratio;
}

int TTerrain::info_get_lod_levels() const {
	return info.lod_levels;
}

int TTerrain::info_get_lod_nodes_count(int p_level) const {
	return quad_tree->get_lod_nodes_count(p_level);
}

int TTerrain::info_get_selected_nodes_count() const {
	return quad_tree->get_selection_count();
}

void TTerrain::set_debug_nodes_aabb_enabled(bool p_enabled) {
	if (p_enabled == debug_nodes_aabb_enabled) {
		return;
	}

	if (p_enabled) {
		debug_nodes_aabb_enabled = true;
		_debug_nodes_aabb_create();
		dirty = true;
	} else {
		_debug_nodes_aabb_free();
	}
}

bool TTerrain::is_debug_nodes_aabb_enabled() const {
	return debug_nodes_aabb_enabled;
}

// void TTerrain::set_chunk_manual_update(bool p_manual) {
// 	chunk_manual_update = p_manual;
// }

// bool TTerrain::is_chunk_manual_update() const {
// 	return chunk_manual_update;
// }

// void TTerrain::set_chunk_active(Vector2i p_active) {
// 	chunk_active = p_active;
// }

// Vector2i TTerrain::get_chunk_active() const {
// 	return chunk_active;
// }

void TTerrain::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			last_transform = get_global_transform();
			_enter_world();
			inside_world = true;
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			_exit_world();
			inside_world = false;
		} break;
		case NOTIFICATION_ENTER_TREE: {
            _update_visibility();
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			Transform3D new_xform = get_global_transform();
			if (new_xform == last_transform) {
				break;
			}

            last_transform = new_xform;
            _update_transform();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
            if (!is_inside_tree()) {
                break;
            }
			_update_visibility();
		} break;
		case NOTIFICATION_PROCESS: {
			_update_viewer();

			if (dirty) {
				_update_chunks();
			}
		} break;
	}
}

void TTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &TTerrain::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &TTerrain::get_chunk_size);
	ClassDB::bind_method(D_METHOD("set_map_scale", "scale"), &TTerrain::set_map_scale);
	ClassDB::bind_method(D_METHOD("get_map_scale"), &TTerrain::get_map_scale);
	ClassDB::bind_method(D_METHOD("set_block_size", "size"), &TTerrain::set_block_size);
	ClassDB::bind_method(D_METHOD("get_block_size"), &TTerrain::get_block_size);
	ClassDB::bind_method(D_METHOD("set_world_blocks", "size"), &TTerrain::set_world_blocks);
	ClassDB::bind_method(D_METHOD("get_world_blocks"), &TTerrain::get_world_blocks);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &TTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &TTerrain::get_material);
	ClassDB::bind_method(D_METHOD("set_lod_detailed_chunks_radius", "radius"), &TTerrain::set_lod_detailed_chunks_radius);
	ClassDB::bind_method(D_METHOD("get_lod_detailed_chunks_radius"), &TTerrain::get_lod_detailed_chunks_radius);
	ClassDB::bind_method(D_METHOD("set_lod_distance_ratio", "ratio"), &TTerrain::set_lod_distance_ratio);
	ClassDB::bind_method(D_METHOD("get_lod_distance_ratio"), &TTerrain::get_lod_distance_ratio);

	ClassDB::bind_method(D_METHOD("info_get_lod_levels"), &TTerrain::info_get_lod_levels);
	ClassDB::bind_method(D_METHOD("info_get_lod_nodes_count", "level"), &TTerrain::info_get_lod_nodes_count);
	ClassDB::bind_method(D_METHOD("info_get_selected_nodes_count"), &TTerrain::info_get_selected_nodes_count);

	ClassDB::bind_method(D_METHOD("set_debug_nodes_aabb_enabled", "enabled"), &TTerrain::set_debug_nodes_aabb_enabled);
	ClassDB::bind_method(D_METHOD("is_debug_nodes_aabb_enabled"), &TTerrain::is_debug_nodes_aabb_enabled);

	// ClassDB::bind_method(D_METHOD("set_chunk_manual_update", "manual"), &TTerrain::set_chunk_manual_update);
	// ClassDB::bind_method(D_METHOD("is_chunk_manual_update"), &TTerrain::is_chunk_manual_update);
	// ClassDB::bind_method(D_METHOD("set_chunk_active", "active"), &TTerrain::set_chunk_active);
	// ClassDB::bind_method(D_METHOD("get_chunk_active"), &TTerrain::get_chunk_active);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size", PROPERTY_HINT_RANGE, vformat("4,%d", MAX_CHUNK_SIZE)), "set_chunk_size", "get_chunk_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "map_scale"), "set_map_scale", "get_map_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "block_size", PROPERTY_HINT_RANGE, vformat("1,%d", MAX_CHUNK_SIZE)), "set_block_size", "get_block_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "world_blocks"), "set_world_blocks", "get_world_blocks");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial"), "set_material", "get_material");

	ADD_GROUP("LOD", "lod_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_detailed_chunks_radius", PROPERTY_HINT_RANGE, "2,16"), "set_lod_detailed_chunks_radius", "get_lod_detailed_chunks_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_distance_ratio", PROPERTY_HINT_RANGE, "1.5,10.0,0.1"), "set_lod_distance_ratio", "get_lod_distance_ratio");
	// ADD_SUBGROUP("Chunk Manual Update", "chunk_");
	// ADD_PROPERTY(PropertyInfo(Variant::BOOL, "chunk_manual_update", PROPERTY_HINT_GROUP_ENABLE), "set_chunk_manual_update", "is_chunk_manual_update");
	// ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "chunk_active"), "set_chunk_active", "get_chunk_active");

	ADD_GROUP("Debug", "debug_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_nodes_aabb_enabled"), "set_debug_nodes_aabb_enabled", "is_debug_nodes_aabb_enabled");
}

void TTerrain::_enter_world() {
	if (!Engine::get_singleton()->is_editor_hint()) {
		_set_viewport_camera();
	}

	if (!mesh_valid) {
		_create_mesh();
	}

	Transform3D xform = get_global_transform();
	RID scenario = get_world_3d()->get_scenario();
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->instance_set_scenario(mm_instance, scenario);
	rs->instance_set_transform(mm_instance, xform);

	if (debug_nodes_aabb_enabled) {
		rs->instance_set_scenario(debug_aabb.instance, scenario);
		rs->instance_set_transform(debug_aabb.instance, xform);
	}
}

void TTerrain::_exit_world() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->instance_set_scenario(mm_instance, RID());

	if (debug_nodes_aabb_enabled) {
		rs->instance_set_scenario(debug_aabb.instance, RID());
	}
}

void TTerrain::_update_visibility() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->instance_set_visible(mm_instance, is_visible_in_tree());

	if (debug_nodes_aabb_enabled) {
		rs->instance_set_visible(debug_aabb.instance, is_visible_in_tree());
	}
}

void TTerrain::_update_transform() {
	Transform3D xform = get_global_transform();
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->instance_set_transform(mm_instance, xform);

	if (debug_nodes_aabb_enabled) {
		rs->instance_set_transform(debug_aabb.instance, xform);
	}
}

void TTerrain::_set_update_distance_tolerance_squared() {
	real_t min_size = MIN(info.map_scale.x, info.map_scale.z);
	update_distance_tolerance_squared = info.chunk_size * min_size * UPDATE_TOLERANCE_FACTOR;
	update_distance_tolerance_squared *= update_distance_tolerance_squared;
}

void TTerrain::_set_lod_levels() {
	if (!camera) {
		return;
	}

	quad_tree->set_lod_levels(camera->get_far(), lod_detailed_chunks_radius);

	if (material.is_valid()) {
		Ref<ImageTexture> morph_texture = quad_tree->get_morph_texture();
		material->set_shader_parameter("morph_data", morph_texture);
	}

	if (debug_nodes_aabb_enabled) {
		_debug_nodes_aabb_set_colors();
	}
}

void TTerrain::_set_viewport_camera() {
	Viewport *viewport = get_viewport();

	if (viewport) {
		camera = viewport->get_camera_3d();
		far_view = -1.0;
	}
}

void TTerrain::_update_viewer() {
	if (!Engine::get_singleton()->is_editor_hint() && use_viewport_camera) {
		Viewport *viewport = get_viewport();

		if (viewport && viewport->get_camera_3d() != camera) {
			_set_viewport_camera();
		}
	}

	if (!camera) {
		dirty = false;
		return;
	}

	if (camera->get_far() != far_view) {
		_set_lod_levels();
		far_view = camera->get_far();
		dirty = true;
	}

	if (dirty) {
		viewer_transform = camera->get_global_transform();
		info.frustum = camera->get_frustum();
	} else {
		Transform3D cam_xform = camera->get_global_transform();

		if (cam_xform.origin.distance_squared_to(viewer_transform.origin) > update_distance_tolerance_squared || !cam_xform.basis.get_euler().is_equal_approx(viewer_transform.basis.get_euler())) {
			viewer_transform = cam_xform;
			info.frustum = camera->get_frustum();
			dirty = true;
		}
	}
}

void TTerrain::_update_chunks() {
	quad_tree->select_nodes(viewer_transform.origin);
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->multimesh_allocate_data(mm_chunks, quad_tree->get_selection_count(), RenderingServer::MULTIMESH_TRANSFORM_3D, true);
	int lod_half = (info.lod_levels + 1) / 2;

	for (int i = 0; i < quad_tree->get_selection_count(); ++i) {
		const TLODQuadTree::QTNode *node = quad_tree->get_selected_node(i);
		const Vector3 bx = Vector3(node->size * info.map_scale.x, 0.0, 0.0);
		const Vector3 by = Vector3(0.0, 1.0, 0.0);
		const Vector3 bz = Vector3(0.0, 0.0, node->size * info.map_scale.z);
		const Vector3 origin = Vector3(node->x * bx.x, node->min_y * info.map_scale.y, node->z * bz.z);
		const Transform3D xform = Transform3D(Basis(bx, by, bz), origin);
		rs->multimesh_instance_set_transform(mm_chunks, i, xform);
		const int lod = node->get_lod_level();
		int color_index = lod / 2 + lod_half * (lod % 2);
		Color lod_color = Color::from_hsv((real_t)color_index / (real_t)info.lod_levels, 0.8, 0.9);
		// const uint32_t flags = uint32_t(node->flags) << 24;
		const uint32_t flags = uint32_t(node->flags) | (1 << 30);
        float encoded_alpha;
        std::memcpy(&encoded_alpha, &flags, sizeof(float));
		Color color = Color(lod_color, encoded_alpha);
		rs->multimesh_instance_set_color(mm_chunks, i, color);
	}

	if (debug_nodes_aabb_enabled) {
		_debug_nodes_aabb_draw();
	}

	dirty = false;
}

void TTerrain::_create_mesh() {
	const int num_points = info.chunk_size + 1;
	PackedVector3Array vertices;
	vertices.resize(num_points * num_points);
	real_t s = 1.0 / (real_t)info.chunk_size;

	// Make vertices.
	for (int iz = 0; iz < num_points; ++iz) {
		for (int ix = 0; ix < num_points; ++ix) {
			real_t x = ix * s;
			real_t z = iz * s;
			vertices.set(ix + iz * num_points, Vector3(x, 0.0, z));
		}
	}

	// Make indices as a combination of 4 subquads so that they
    // can be rendered separately when needed.
	int index = 0;
	const int half = num_points / 2;
	PackedInt32Array indices;
	indices.resize(6 * info.chunk_size * info.chunk_size);
	bool tri_a = true;

	// Top left.
	for (int iz = 0; iz < half; ++iz) {
		for (int ix = 0; ix < half; ++ix) {
			const int i1 = ix + num_points * iz;
			const int i2 = ix + 1 + num_points * iz;
			const int i3 = ix + num_points * (iz + 1);
			const int i4 = ix + 1 + num_points * (iz + 1);

			if (tri_a) {
				indices.set(index++, i1);
				indices.set(index++, i2);
				indices.set(index++, i3);
				indices.set(index++, i2);
				indices.set(index++, i4);
				indices.set(index++, i3);
			} else {
				indices.set(index++, i1);
				indices.set(index++, i2);
				indices.set(index++, i4);
				indices.set(index++, i1);
				indices.set(index++, i4);
				indices.set(index++, i3);
			}

			tri_a = !tri_a;
		}
	}

	// Top right.
	for (int iz = 0; iz < half; ++iz) {
		for (int ix = half; ix < info.chunk_size; ++ix) {
			const int i1 = ix + num_points * iz;
			const int i2 = ix + 1 + num_points * iz;
			const int i3 = ix + num_points * (iz + 1);
			const int i4 = ix + 1 + num_points * (iz + 1);

			if (tri_a) {
				indices.set(index++, i1);
				indices.set(index++, i2);
				indices.set(index++, i3);
				indices.set(index++, i2);
				indices.set(index++, i4);
				indices.set(index++, i3);
			} else {
				indices.set(index++, i1);
				indices.set(index++, i2);
				indices.set(index++, i4);
				indices.set(index++, i1);
				indices.set(index++, i4);
				indices.set(index++, i3);
			}

			tri_a = !tri_a;
		}
	}

	// Bottom left.
	for (int iz = half; iz < info.chunk_size; ++iz) {
		for (int ix = 0; ix < half; ++ix) {
			const int i1 = ix + num_points * iz;
			const int i2 = ix + 1 + num_points * iz;
			const int i3 = ix + num_points * (iz + 1);
			const int i4 = ix + 1 + num_points * (iz + 1);

			if (tri_a) {
				indices.set(index++, i1);
				indices.set(index++, i2);
				indices.set(index++, i3);
				indices.set(index++, i2);
				indices.set(index++, i4);
				indices.set(index++, i3);
			} else {
				indices.set(index++, i1);
				indices.set(index++, i2);
				indices.set(index++, i4);
				indices.set(index++, i1);
				indices.set(index++, i4);
				indices.set(index++, i3);
			}

			tri_a = !tri_a;
		}
	}

	// Bottom right.
	for (int iz = half; iz < info.chunk_size; ++iz) {
		for (int ix = half; ix < info.chunk_size; ++ix) {
			const int i1 = ix + num_points * iz;
			const int i2 = ix + 1 + num_points * iz;
			const int i3 = ix + num_points * (iz + 1);
			const int i4 = ix + 1 + num_points * (iz + 1);

			if (tri_a) {
				indices.set(index++, i1);
				indices.set(index++, i2);
				indices.set(index++, i3);
				indices.set(index++, i2);
				indices.set(index++, i4);
				indices.set(index++, i3);
			} else {
				indices.set(index++, i1);
				indices.set(index++, i2);
				indices.set(index++, i4);
				indices.set(index++, i1);
				indices.set(index++, i4);
				indices.set(index++, i3);
			}

			tri_a = !tri_a;
		}
	}

	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->mesh_clear(mesh);
	Array arrays;
	arrays.resize(RenderingServer::ARRAY_MAX);
	arrays[RenderingServer::ARRAY_VERTEX] = vertices;
	arrays[RenderingServer::ARRAY_INDEX] = indices;
	rs->mesh_add_surface_from_arrays(mesh, RenderingServer::PRIMITIVE_TRIANGLES, arrays);
	mesh_valid = true;

	if (material.is_valid()) {
		rs->mesh_surface_set_material(mesh, 0, material->get_rid());
	}
}

void TTerrain::_debug_nodes_aabb_create() {
	// Create debug mesh.
	const PackedVector3Array vertices = {
		// Beam Top Back.
		Vector3(0,1,0), Vector3(1,1,0), Vector3(1,1,0), Vector3(0,1,0), // [0-3]
		Vector3(0,1,0), Vector3(1,1,0), // [4-5]
		Vector3(0,1,0), Vector3(1,1,0), // [6-7]
		// Beam Top Left.
		Vector3(0,1,0), Vector3(0,1,1), Vector3(0,1,1), // [8-10]
		Vector3(0,1,1), // [11]
		Vector3(0,1,0), Vector3(0,1,1), // [12-13]
		// Beam Top Front.
		Vector3(0,1,1), Vector3(1,1,1), Vector3(1,1,1), // [14-16]
		Vector3(1,1,1), // [17]
		Vector3(0,1,1), Vector3(1,1,1), // [18-19]
		// Beam Top Right.
		Vector3(1,1,0), Vector3(1,1,1), // [20,21]
		Vector3(1,1,0), Vector3(1,1,1), // [22-23]
		// Beam Back Left.
		Vector3(0,1,0), // [24]
		Vector3(0,0,0), Vector3(0,0,0), Vector3(0,0,0), Vector3(0,0,0), // [25-28]
		// Beam Back Right.
		Vector3(1,1,0), // [29]
		Vector3(1,0,0), Vector3(1,0,0), Vector3(1,0,0), Vector3(1,0,0), // [30-33]
		// Beam Front Left.
		Vector3(0,1,1), // [34]
		Vector3(0,0,1), Vector3(0,0,1), Vector3(0,0,1), Vector3(0,0,1), // [35-38]
		// Beam Front Right.
		Vector3(1,1,1), // [39]
		Vector3(1,0,1), Vector3(1,0,1), Vector3(1,0,1), Vector3(1,0,1), // [40-43]
	};
	const PackedInt32Array indices = {
		// Beam Top Back.
		0,1,2, 0,2,3,
		0,4,1, 4,5,1,
		4,6,5, 6,7,5,
		3,2,6, 2,7,6,
		// Beam Top Left.
		0,8,10, 0,10,9,
		0,9,11, 0,11,4,
		4,11,13, 4,13,12,
		8,12,10, 12,13,10,
		// Beam Top Front.
		9,14,15, 14,16,15,
		9,15,11, 15,17,11,
		11,17,18, 18,17,19,
		18,19,14, 14,19,16,
		// Beam Top Right.
		1,15,20, 20,15,21,
		1,5,15, 5,17,15,
		5,23,17, 5,22,23,
		20,21,22, 22,21,23,
		// Beam Back Left.
		0,26,8, 0,25,26,
		0,3,27, 0,27,25,
		3,24,28, 3,28,27,
		24,8,26, 24,26,28,
		// Beam Back Right.
		1,20,30, 30,20,31,
		1,30,2, 2,30,32,
		29,2,32, 29,32,33,
		20,29,33, 20,33,31,
		// Beam Front Left.
		9,10,36, 9,36,35,
		14,9,35, 14,35,37,
		14,37,38, 14,38,34,
		10,34,38, 10,38,36,
		// Beam Front Right.
		15,40,21, 21,40,41,
		15,16,42, 15,42,40,
		16,39,42, 39,43,42,
		39,21,41, 39,41,43,
	};
	const PackedColorArray colors = {
		// Beam Top Back.
		Color(0.5,0.5,0.5), Color(0.5,0.5,0.5), Color(0.5,0.5,1.0), Color(0.5,0.5,1.0),
		Color(0.5,0.0,0.5), Color(0.5,0.0,0.5),
		Color(0.5,0.0,1.0), Color(0.5,0.0,1.0),
		// Beam Top Left.
		Color(1.0,0.5,0.5), Color(0.5,0.5,0.5), Color(1.0,0.5,0.5),
		Color(0.5,0.0,0.5),
		Color(1.0,0.0,0.5), Color(1.0,0.0,0.5),
		// Beam Top Front.
		Color(0.5,0.5,0.0), Color(0.5,0.5,0.5), Color(0.5,0.5,0.0),
		Color(0.5,0.0,0.5),
		Color(0.5,0.0,0.0), Color(0.5,0.0,0.0),
		// Beam Top Right.
		Color(0.0,0.5,0.5), Color(0.0,0.5,0.5),
		Color(0.0,0.0,0.5), Color(0.0, 0.0, 0.5),
		// Beam Back Left.
		Color(1.0,0.5,1.0),
		Color(0.5,0.5,0.5), Color(1.0,0.5,0.5), Color(0.5,0.5,1.0), Color(1.0,0.5,1.0),
		// Beam Back Right.
		Color(0.0,0.5,1.0),
		Color(0.5,0.5,0.5), Color(0.0,0.5,0.5), Color(0.5,0.5,1.0), Color(0.0,0.5,1.0),
		// Beam Front Left.
		Color(1.0,0.5,0.0),
		Color(0.5, 0.5, 0.5), Color(1.0,0.5,0.5), Color(0.5,0.5,0.0), Color(1.0,0.5,0.0),
		// Beam Front Right.
		Color(0.0,0.5,0.0),
		Color(0.5,0.5,0.5), Color(0.0,0.5,0.5), Color(0.5,0.5,0.0), Color(0.0,0.5,0.0),
	};
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_INDEX] = indices;
	arrays[Mesh::ARRAY_COLOR] = colors;
	RenderingServer *const rs = RenderingServer::get_singleton();
	debug_aabb.mesh = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(debug_aabb.mesh, RenderingServer::PRIMITIVE_TRIANGLES, arrays);
	debug_aabb.shader = rs->shader_create();
	const String shader_code = R"(
shader_type spatial;
render_mode unshaded, world_vertex_coords;

uniform float width = 0.1;

varying vec3 color;

void vertex() {
	vec3 displacement = (2.0 * COLOR.xyz - 1.0) * width;
	VERTEX += displacement;
	color = INSTANCE_CUSTOM.rgb;
}

void fragment() {
	ALBEDO = color;
}
)";
	rs->shader_set_code(debug_aabb.shader, shader_code);
	debug_aabb.material = rs->material_create();
	rs->material_set_shader(debug_aabb.material, debug_aabb.shader);
	rs->mesh_surface_set_material(debug_aabb.mesh, 0, debug_aabb.material);
	debug_aabb.multimesh = rs->multimesh_create();
	rs->multimesh_set_mesh(debug_aabb.multimesh, debug_aabb.mesh);

	if (inside_world) {
		debug_aabb.instance = rs->instance_create2(debug_aabb.multimesh, get_world_3d()->get_scenario());
		rs->instance_set_visible(debug_aabb.instance, is_visible_in_tree());
		rs->instance_set_transform(debug_aabb.instance, get_global_transform());
	} else {
		debug_aabb.instance = rs->instance_create();
		rs->instance_set_base(debug_aabb.instance, debug_aabb.multimesh);
	}

	rs->instance_geometry_set_cast_shadows_setting(debug_aabb.instance, RenderingServer::SHADOW_CASTING_SETTING_OFF);
	_debug_nodes_aabb_set_colors();
}

void TTerrain::_debug_nodes_aabb_free() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	SERVER_FREE(rs, debug_aabb.instance);
	SERVER_FREE(rs, debug_aabb.multimesh);
	SERVER_FREE(rs, debug_aabb.mesh);
	SERVER_FREE(rs, debug_aabb.material);
	SERVER_FREE(rs, debug_aabb.shader);
	debug_aabb.lod_colors.clear();
	debug_nodes_aabb_enabled = false;
}

void TTerrain::_debug_nodes_aabb_draw() const {
	RenderingServer *const rs = RenderingServer::get_singleton();
	int num_nodes = quad_tree->get_selection_count();
	rs->multimesh_allocate_data(debug_aabb.multimesh, num_nodes, RenderingServer::MULTIMESH_TRANSFORM_3D, false, true);

	for (int i = 0; i < num_nodes; ++i) {
		int lod = quad_tree->get_selected_node_lod(i);
		real_t margin = DEBUG_AABB_LOD0_MARGIN + lod * DEBUG_AABB_MARGIN_LOD_SCALE_FACTOR;
		AABB node_aabb = quad_tree->get_selected_node_aabb(i);
		Vector3 bx = Vector3(node_aabb.size.x, 0, 0);
		Vector3 by = Vector3(0, node_aabb.size.y + 2.0 * margin, 0);
		Vector3 bz = Vector3(0, 0, node_aabb.size.z);
		Transform3D xform = Transform3D(Basis(bx, by, bz), node_aabb.position - Vector3(0, margin, 0));
		rs->multimesh_instance_set_transform(debug_aabb.multimesh, i, xform);
		rs->multimesh_instance_set_custom_data(debug_aabb.multimesh, i, debug_aabb.lod_colors[lod]);
	}
}

void TTerrain::_debug_nodes_aabb_set_colors() {
	if (debug_aabb.lod_colors.size() == info.lod_levels) {
		return;
	}

	debug_aabb.lod_colors.resize(info.lod_levels);
	int half = (info.lod_levels + 1) / 2;

	for (int i = 0; i < info.lod_levels; ++i) {
		int index = i / 2 + half * (i % 2);
		Color color = Color::from_hsv((real_t)index / (real_t)info.lod_levels, 0.8, 0.9);
		debug_aabb.lod_colors.set(i, color);
	}
}

TTerrain::TTerrain() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	mesh = rs->mesh_create();
	mm_chunks = rs->multimesh_create();
	rs->multimesh_set_mesh(mm_chunks, mesh);
	mm_instance = rs->instance_create();
	rs->instance_set_base(mm_instance, mm_chunks);
	set_notify_transform(true);
	_set_update_distance_tolerance_squared();
	set_process(true);
	quad_tree.instantiate();
	quad_tree->set_info(&info);

// 	include.instantiate();
// 	const String include_code = R"(
// float LOD = 1.0;
// )";
// 	include->set_code(include_code);
// 	include->set_path("res://TERRAIN", true);
// 	print_line(vformat("Include created. %s", include->to_string()));
}

TTerrain::~TTerrain() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	SERVER_FREE(rs, mm_instance);
	SERVER_FREE(rs, mm_chunks);
	SERVER_FREE(rs, mesh);

	if (debug_nodes_aabb_enabled) {
		_debug_nodes_aabb_free();
	}
}