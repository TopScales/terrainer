/**
 * terrain.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#include "terrain.h"

// #include "utils/macros.h"
// #include "utils/math.h"

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

using namespace Terrainer;

void Terrain::set_camera(Camera3D *p_camera) {
	if (p_camera != camera) {
		camera = p_camera;
		far_view = -1.0;
	}

	use_viewport_camera = camera == nullptr;
}

void Terrain::set_storage(const Ref<MapStorage> &p_storage) {
	if (storage.is_valid()) {
		storage->disconnect_changed(callable_mp(this, &Terrain::_storage_changed));
		storage->disconnect(MapStorage::path_changed, callable_mp(this, &Terrain::_storage_path_changed));
	}

	storage = p_storage;

	if (storage.is_valid()) {
		storage->connect_changed(callable_mp(this, &Terrain::_storage_changed));
		storage->connect(MapStorage::path_changed, callable_mp(this, &Terrain::_storage_path_changed));
		storage_status = storage->load_headers();
		_storage_changed();
	} else {
		storage_status = ERR_CANT_ACQUIRE_RESOURCE;
	}

	update_configuration_warnings();
}

Ref<MapStorage> Terrain::get_storage() const {
	return storage;
}

void Terrain::set_map_scale(const Vector3 &p_scale) {
	ERR_FAIL_COND_EDMSG(p_scale.x <= 0.0 || p_scale.y <= 0.0 || p_scale.z <= 0.0, "Scale must be positive.");
	map_scale = p_scale;

	if (storage.is_valid()) {
		quad_tree.set_map_info(storage->get_chunk_size(), storage->get_region_size(), world_regions, map_scale);
		_set_update_distance_tolerance_squared();
		_set_lod_levels();
	}

	dirty = true;
}

Vector3 Terrain::get_map_scale() const {
	return map_scale;
}

void Terrain::set_world_regions(const Vector2i &p_regions) {
	ERR_FAIL_COND_EDMSG(p_regions.x <= 0 || p_regions.y <= 0, "World regions must be positive.");
	world_regions = p_regions;

	if (storage.is_valid()) {
		quad_tree.set_map_info(storage->get_chunk_size(), storage->get_region_size(), world_regions, map_scale);
		_set_lod_levels();
	}
}

Vector2i Terrain::get_world_regions() const {
	return world_regions;
}

// void Terrain::set_material(const Ref<ShaderMaterial> &p_material) {
// 	material = p_material;

// 	if (mesh_valid) {
// 		RenderingServer::get_singleton()->mesh_surface_set_material(mesh, 0, material->get_rid());
// 	}

// 	if (material.is_valid() && world_info) {
// 		material->set_shader_parameter("grid_const", Vector2(0.5 * (real_t)world_info->chunk_size, 2.0 / (real_t)world_info->chunk_size));
// 	}
// }

// Ref<ShaderMaterial> Terrain::get_material() const {
// 	return material;
// }

void Terrain::set_lod_detailed_chunks_radius(int p_radius) {
	lod_detailed_chunks_radius = p_radius;
	_set_lod_levels();
}

int Terrain::get_lod_detailed_chunks_radius() const {
	return lod_detailed_chunks_radius;
}

void Terrain::set_lod_distance_ratio(real_t p_ratio) {
	ERR_FAIL_COND_EDMSG(p_ratio < 1.0, "LOD level distance ratio must be at least 1.");
	quad_tree.lod_distance_ratio = p_ratio;
	_set_lod_levels();
}

real_t Terrain::get_lod_distance_ratio() const {
	return quad_tree.lod_distance_ratio;
}

int Terrain::info_get_lod_levels() const {
	return quad_tree.lod_levels;
}

int Terrain::info_get_lod_nodes_count(int p_level) const {
	return quad_tree.get_lod_nodes_count(p_level);
}

int Terrain::info_get_selected_nodes_count() const {
	return quad_tree.selection_count;
}

void Terrain::set_debug_nodes_aabb_enabled(bool p_enabled) {
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

bool Terrain::is_debug_nodes_aabb_enabled() const {
	return debug_nodes_aabb_enabled;
}

void Terrain::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			last_transform = get_global_transform();
			_enter_world();
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			_exit_world();
		} break;
		case NOTIFICATION_ENTER_TREE: {
            _update_visibility();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (storage.is_valid()) {
				storage->stop_io();
			}
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
		case NOTIFICATION_INTERNAL_PROCESS: {
			_update_viewer(get_process_delta_time());

			if (dirty) {
				_update_chunks();
			}

			storage->process();
		} break;
	}
}

void Terrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_storage", "storage"), &Terrain::set_storage);
	ClassDB::bind_method(D_METHOD("get_storage"), &Terrain::get_storage);
	ClassDB::bind_method(D_METHOD("set_map_scale", "scale"), &Terrain::set_map_scale);
	ClassDB::bind_method(D_METHOD("get_map_scale"), &Terrain::get_map_scale);
	ClassDB::bind_method(D_METHOD("set_world_regions", "regions"), &Terrain::set_world_regions);
	ClassDB::bind_method(D_METHOD("get_world_regions"), &Terrain::get_world_regions);
// 	ClassDB::bind_method(D_METHOD("set_material", "material"), &Terrain::set_material);
// 	ClassDB::bind_method(D_METHOD("get_material"), &Terrain::get_material);
	ClassDB::bind_method(D_METHOD("set_lod_detailed_chunks_radius", "radius"), &Terrain::set_lod_detailed_chunks_radius);
	ClassDB::bind_method(D_METHOD("get_lod_detailed_chunks_radius"), &Terrain::get_lod_detailed_chunks_radius);
	ClassDB::bind_method(D_METHOD("set_lod_distance_ratio", "ratio"), &Terrain::set_lod_distance_ratio);
	ClassDB::bind_method(D_METHOD("get_lod_distance_ratio"), &Terrain::get_lod_distance_ratio);

	ClassDB::bind_method(D_METHOD("info_get_lod_levels"), &Terrain::info_get_lod_levels);
	ClassDB::bind_method(D_METHOD("info_get_lod_nodes_count", "level"), &Terrain::info_get_lod_nodes_count);
	ClassDB::bind_method(D_METHOD("info_get_selected_nodes_count"), &Terrain::info_get_selected_nodes_count);

	ClassDB::bind_method(D_METHOD("set_debug_nodes_aabb_enabled", "enabled"), &Terrain::set_debug_nodes_aabb_enabled);
	ClassDB::bind_method(D_METHOD("is_debug_nodes_aabb_enabled"), &Terrain::is_debug_nodes_aabb_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "storage", PROPERTY_HINT_RESOURCE_TYPE, "MapStorage"), "set_storage", "get_storage");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "map_scale"), "set_map_scale", "get_map_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "world_regions"), "set_world_regions", "get_world_regions");
// 	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial"), "set_material", "get_material");

	ADD_GROUP("LOD", "lod_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_detailed_chunks_radius", PROPERTY_HINT_RANGE, "1,16"), "set_lod_detailed_chunks_radius", "get_lod_detailed_chunks_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_distance_ratio", PROPERTY_HINT_RANGE, "1.5,10.0,0.1"), "set_lod_distance_ratio", "get_lod_distance_ratio");

	ADD_GROUP("Debug", "debug_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_nodes_aabb_enabled"), "set_debug_nodes_aabb_enabled", "is_debug_nodes_aabb_enabled");
}

PackedStringArray Terrain::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (storage.is_null()) {
		warnings.push_back(RTR("MapStorage resource is missing."));
	} else if (!storage->is_directory_set()) {
		warnings.push_back(RTR("Set a storage directory in the MapStorage resource."));
	}

	return warnings;
}

void Terrain::_enter_world() {
	if (storage.is_null()) {
		Ref<MapStorage> new_storage;
		new_storage.instantiate();
		set_storage(new_storage);
	}

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

	inside_world = true;
}

void Terrain::_exit_world() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->instance_set_scenario(mm_instance, RID());

	if (debug_nodes_aabb_enabled) {
		rs->instance_set_scenario(debug_aabb.instance, RID());
	}

	inside_world = false;
}

void Terrain::_update_visibility() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->instance_set_visible(mm_instance, is_visible_in_tree());

	if (debug_nodes_aabb_enabled) {
		rs->instance_set_visible(debug_aabb.instance, is_visible_in_tree());
	}

}

void Terrain::_update_transform() {
	Transform3D xform = get_global_transform();
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->instance_set_transform(mm_instance, xform);

	if (debug_nodes_aabb_enabled) {
		rs->instance_set_transform(debug_aabb.instance, xform);
	}
}

void Terrain::_update_viewer(double p_delta) {
	if (!Engine::get_singleton()->is_editor_hint() && use_viewport_camera) {
		Viewport *viewport = get_viewport();

		if (viewport && viewport->get_camera_3d() != camera) {
			_set_viewport_camera();
		}
	}

	if (!camera || storage_status != OK) {
		dirty = false;
		return;
	}

	if (camera->get_far() != far_view) {
		_set_lod_levels();
		far_view = camera->get_far();
	}

	Vector3 prev_pos = viewer_transform.origin;

	if (dirty) {
		viewer_transform = camera->get_global_transform();
		quad_tree.frustum = camera->get_frustum();
	} else {
		Transform3D cam_xform = camera->get_global_transform();

		if (cam_xform.origin.distance_squared_to(viewer_transform.origin) > update_distance_tolerance_squared || !cam_xform.basis.get_euler().is_equal_approx(viewer_transform.basis.get_euler())) {
			viewer_transform = cam_xform;
			quad_tree.frustum = camera->get_frustum();
			dirty = true;
		}
	}

	if (dirty) {
		Vector3 pos = viewer_transform.origin;
		Vector3 vel = pos.direction_to(prev_pos) / p_delta;
		Vector3 forward = viewer_transform.basis.get_column(2);
		storage->update_viewer(pos, vel, forward);
	}
}

void Terrain::_update_chunks() {
	const int sector_size = quad_tree.sector_size * storage->get_chunk_size();
	const real_t sector_size_x = sector_size * map_scale.x;
	const real_t sector_size_z = sector_size * map_scale.z;
	const Vector3 viewer_position = viewer_transform.origin;
	const real_t far_squared = far_view * far_view;
	quad_tree.selection_count = 0;

	for (uint16_t iz = 0; iz < quad_tree.sector_count_z; ++iz) {
		for (uint16_t ix = 0; ix < quad_tree.sector_count_x; ++ix) {
			const Vector3 sector_pos = Vector3(sector_size_x * ix, 0.0, sector_size_z * iz) + quad_tree.world_offset;
			real_t dx = sector_pos.x - viewer_position.x;
			dx = MIN(abs(dx), abs(dx + sector_size_x));
			real_t dz = sector_pos.z - viewer_position.z;
			dz = MIN(abs(dz), abs(dx + sector_size_z));

			if (dx * dx + dz * dz < far_squared) {
				CellKey sector = {ix, iz};

				if (storage->is_sector_loaded(sector)) {
					quad_tree.select_sector_nodes(viewer_transform.origin, sector, storage);
				} else {
					LODQuadTree::NodeSelectionResult result = quad_tree.select_sector_nodes(viewer_transform.origin, sector, storage, quad_tree.lod_levels - 1);

					if (result != LODQuadTree::NodeSelectionResult::OutOfRange) {
						storage->load_minmax(sector, result != LODQuadTree::NodeSelectionResult::OutOfFrustum);
					}
				}
			}
		}
	}

	dirty = false;

	if (quad_tree.selection_count == 0) {
		return;
	}

	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->multimesh_allocate_data(mm_chunks, quad_tree.selection_count, RenderingServer::MULTIMESH_TRANSFORM_3D, true);
	int lod_half = (quad_tree.lod_levels + 1) / 2;
	int instance_index = 0;

	for (int i = 0; i < quad_tree.selection_count; ++i) {
		const LODQuadTree::QTNode *node = quad_tree.get_selected_node(i);
		int lod = node->get_lod_level();
		int texture_layer = storage->get_node_texture_layer(node->key, lod);
		// int lod_diff = quad_tree.lod_levels - lod - 1;
		// int x = node->x >> lod_diff;
// 		int z = node->z >> lod_diff;
// 		Vector2i root_node = Vector2i(x, z);
// 		int texture_layer = 0;
// 		bool node_ready = storage->get_node_texture_layer(root_node, texture_layer);
// 		int node_size = 1 << lod_diff;
// 		Vector2i uv_shift = Vector2i(node->x - x * node_size, node->z - z * node_size);

// 		if (node_ready) {
// 			_configure_chunk_mesh(rs, node, instance_index);
// 			instance_index++;
// 		}
	}

	if (debug_nodes_aabb_enabled) {
		_debug_nodes_aabb_draw();
	}

	rs->multimesh_set_visible_instances(mm_chunks, instance_index);
}

void Terrain::_set_viewport_camera() {
	Viewport *viewport = get_viewport();

	if (viewport) {
		camera = viewport->get_camera_3d();
		far_view = -1.0;
	}
}

void Terrain::_create_mesh() {
	if (storage.is_null()) {
		return;
	}

	const int chunk_size = storage->get_chunk_size();
	const int num_points = chunk_size + 1;
	PackedVector3Array vertices;
	vertices.resize(num_points * num_points);
	const real_t s = 1.0 / (real_t)chunk_size;

	// Make vertices.
	for (int iz = 0; iz < num_points; ++iz) {
		for (int ix = 0; ix < num_points; ++ix) {
			real_t x = ix * s;
			real_t z = iz * s;
			vertices.set(ix + iz * num_points, Vector3(x, 0.0, z));
		}
	}

	PackedInt32Array indices;
	indices.resize(6 * chunk_size * chunk_size);
	bool tri_a = true;
	int index = 0;

	for (int iz = 0; iz < chunk_size; ++iz) {
		for (int ix = 0; ix < chunk_size; ++ix) {
			const int i1 = ix + num_points * iz;
			const int i2 = i1 + 1;
			const int i3 = i1 + num_points;
			const int i4 = i3 + 1;

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

		tri_a = !tri_a;
	}

	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->mesh_clear(mesh);
	Array arrays;
	arrays.resize(RenderingServer::ARRAY_MAX);
	arrays[RenderingServer::ARRAY_VERTEX] = vertices;
	arrays[RenderingServer::ARRAY_INDEX] = indices;
	rs->mesh_add_surface_from_arrays(mesh, RenderingServer::PRIMITIVE_TRIANGLES, arrays);
	mesh_valid = true;

// 	if (material.is_valid()) {
// 		rs->mesh_surface_set_material(mesh, 0, material->get_rid());
// 	}
}

void Terrain::_set_lod_levels() {
	if (!camera || storage_status != OK) {
		return;
	}

	quad_tree.set_lod_levels(camera->get_far(), lod_detailed_chunks_radius);
	storage->allocate_minmax(quad_tree.sector_size, quad_tree.lod_levels, world_regions, map_scale, far_view);
	dirty = true;

// 	if (material.is_valid()) {
// 		Ref<ImageTexture> morph_texture = quad_tree->get_morph_texture();
// 		material->set_shader_parameter("morph_data", morph_texture);
// 	}

// 	if (storage_status == OK) {
// 		storage->setup(info);
// 	}

	if (debug_nodes_aabb_enabled) {
		_debug_nodes_aabb_set_colors();
	}
}

void Terrain::_storage_changed() {
	_set_update_distance_tolerance_squared();
	quad_tree.set_map_info(storage->get_chunk_size(), storage->get_region_size(), world_regions, map_scale);
	_set_lod_levels();

	if (inside_world) {
		_create_mesh();
	} else {
		mesh_valid = false;
	}

// 	if (material.is_valid()) {
// 		material->set_shader_parameter("grid_const", Vector2(0.5 * (real_t)world_info->chunk_size, 2.0 / (real_t)world_info->chunk_size));
// 	}
}

void Terrain::_storage_path_changed() {
	storage_status = storage->load_headers();
	dirty = true;
	update_configuration_warnings();
}

void Terrain::_set_update_distance_tolerance_squared() {
	if (storage.is_null()) {
		return;
	}

	real_t min_size = MIN(map_scale.x, map_scale.z);
	update_distance_tolerance_squared = storage->get_chunk_size() * min_size * UPDATE_TOLERANCE_FACTOR;
	update_distance_tolerance_squared *= update_distance_tolerance_squared;
}

// void Terrain::_configure_chunk_mesh(RenderingServer *p_rs, const TLODQuadTree::QTNode *p_node, int p_instance_index) {
// 	const Transform3D xform = quad_tree->get_node_transform(p_node);
// 	p_rs->multimesh_instance_set_transform(mm_chunks, p_instance_index, xform);
	// const int lod = p_node->get_lod_level();
	// int color_index = lod / 2 + lod_half * (lod % 2);
	// Color lod_color = Color::from_hsv((real_t)color_index / (real_t)info.lod_levels, 0.8, 0.9);
	// const uint32_t flags = uint32_t(node->flags) << 24;
	// const uint32_t flags = uint32_t(node->flags) | (1 << 30);
	// float encoded_alpha;
	// std::memcpy(&encoded_alpha, &flags, sizeof(float));
	// Color color = Color(lod_color, encoded_alpha);
	// rs->multimesh_instance_set_color(mm_chunks, i, color);
// }

void Terrain::_debug_nodes_aabb_create() {
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

void Terrain::_debug_nodes_aabb_free() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->free_rid(debug_aabb.instance);
	rs->free_rid(debug_aabb.multimesh);
	rs->free_rid(debug_aabb.mesh);
	rs->free_rid(debug_aabb.material);
	rs->free_rid(debug_aabb.shader);
	debug_aabb.lod_colors.clear();
	debug_nodes_aabb_enabled = false;
}

void Terrain::_debug_nodes_aabb_draw() const {
	RenderingServer *const rs = RenderingServer::get_singleton();
	int num_nodes = quad_tree.selection_count;
	rs->multimesh_allocate_data(debug_aabb.multimesh, num_nodes, RenderingServer::MULTIMESH_TRANSFORM_3D, false, true);

	for (int i = 0; i < num_nodes; ++i) {
		const LODQuadTree::QTNode *node = quad_tree.get_selected_node(i);
		const int lod = node->get_lod_level();
		const real_t margin = DEBUG_AABB_LOD0_MARGIN + lod * DEBUG_AABB_MARGIN_LOD_SCALE_FACTOR;
		const Transform3D xform = quad_tree.get_node_transform(node);
		rs->multimesh_instance_set_transform(debug_aabb.multimesh, i, xform);
		rs->multimesh_instance_set_custom_data(debug_aabb.multimesh, i, debug_aabb.lod_colors[lod]);
	}
}

void Terrain::_debug_nodes_aabb_set_colors() {
	if (debug_aabb.lod_colors.size() == quad_tree.lod_levels) {
		return;
	}

	debug_aabb.lod_colors.resize(quad_tree.lod_levels);
	const int half = (quad_tree.lod_levels + 1) / 2;

	for (int i = 0; i < quad_tree.lod_levels; ++i) {
		int index = i / 2 + half * (i % 2);
		Color color = Color::from_hsv((real_t)index / (real_t)quad_tree.lod_levels, 0.8, 0.9);
		debug_aabb.lod_colors.set(i, color);
	}
}

Terrain::Terrain() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	mesh = rs->mesh_create();
	mm_chunks = rs->multimesh_create();
	rs->multimesh_set_mesh(mm_chunks, mesh);
	mm_instance = rs->instance_create();
	rs->instance_set_base(mm_instance, mm_chunks);
	set_notify_transform(true);
	set_process_internal(true);

// // 	include.instantiate();
// // 	const String include_code = R"(
// // float LOD = 1.0;
// // )";
// // 	include->set_code(include_code);
// // 	include->set_path("res://TERRAIN", true);
// // 	print_line(vformat("Include created. %s", include->to_string()));
}

Terrain::~Terrain() {
	RenderingServer *const rs = RenderingServer::get_singleton();
	rs->free_rid(mm_instance);
	rs->free_rid(mm_chunks);
	rs->free_rid(mesh);

	if (debug_nodes_aabb_enabled) {
		_debug_nodes_aabb_free();
	}
}