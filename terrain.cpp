#include "terrain.h"

#include "scene/main/viewport.h"

const real_t TTerrain::UPDATE_TOLERANCE_FACTOR = 0.05;

void TTerrain::set_camera(Camera3D *p_camera) {
	camera = p_camera;

	if (camera) {
		update_view = true;
	}
}

void TTerrain::set_chunk_size(int p_size) {
	ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain chunk size must be greater than zero.");

	if (p_size > MAX_CHUNK_SIZE || p_size == chunk_size) {
		return;
	}

	const int size = t_round_po2(p_size, chunk_size);

	if (size != chunk_size) {
		chunk_size = size;
		update_distance_tolerance_squared = chunk_size * quad_size * UPDATE_TOLERANCE_FACTOR;
		update_distance_tolerance_squared *= update_distance_tolerance_squared;
		// dirty = true;
	}
}

int TTerrain::get_chunk_size() const {
	return chunk_size;
}

void TTerrain::set_chunk_world_offset(Vector2i p_offset) {
	chunk_world_offset = p_offset;
}

Vector2i TTerrain::get_chunk_world_offset() const {
	return chunk_world_offset;
}

void TTerrain::set_centered(bool p_centered) {
	centered = p_centered;
}

bool TTerrain::is_centered() const {
	return centered;
}

void TTerrain::set_unit_size(real_t p_size) {
	quad_size = p_size;
	update_distance_tolerance_squared = chunk_size * quad_size * UPDATE_TOLERANCE_FACTOR;
	update_distance_tolerance_squared *= update_distance_tolerance_squared;
}

real_t TTerrain::get_unit_size() const {
	return quad_size;
}

void TTerrain::set_chunk_manual_update(bool p_manual) {
	chunk_manual_update = p_manual;
}

bool TTerrain::is_chunk_manual_update() const {
	return chunk_manual_update;
}

void TTerrain::set_chunk_active(Vector2i p_active) {
	chunk_active = p_active;
}

Vector2i TTerrain::get_chunk_active() const {
	return chunk_active;
}

void TTerrain::set_mesh_quad_tree_leaf_size(int p_size) {
	ERR_FAIL_COND_EDMSG(p_size < MIN_QUAD_TREE_LEAD_SIZE, "Quad Tree lead size must be at least 2.");
	const int size = t_round_po2(p_size, data.quad_tree_leaf_size);

	if (size != data.quad_tree_leaf_size) {
		data.quad_tree_leaf_size = size;
		_check_mesh_render_resolution();
		// dirty = true;
	}
}

int TTerrain::get_mesh_quad_tree_leaf_size() const {
	return data.quad_tree_leaf_size;
}

void TTerrain::set_mesh_render_resolution_multiplier(int p_multiplier) {
	ERR_FAIL_COND_EDMSG(p_multiplier < 1, "Render resolution multiplier must be at least 1.");
	const int multiplier = t_round_po2(p_multiplier, mesh_render_resolution_multiplier);

	if (multiplier != mesh_render_resolution_multiplier) {
		mesh_render_resolution_multiplier = multiplier;
		_check_mesh_render_resolution();
		// Update
	}
}

int TTerrain::get_mesh_render_resolution_multiplier() const {
	return mesh_render_resolution_multiplier;
}

void TTerrain::set_mesh_lod_level_count(int p_count) {
	ERR_FAIL_COND_EDMSG(p_count < 2, "LOD level count must be at least 2.");
	ERR_FAIL_COND_EDMSG(p_count > TLODQuadTree::MAX_LOD_LEVELS, vformat("LOD level count must be at most %d.", TLODQuadTree::MAX_LOD_LEVELS));

	if (p_count != data.lod_level_count) {
		data.lod_level_count = p_count;
		// Update
	}
}

int TTerrain::get_mesh_lod_level_count() const {
	return data.lod_level_count;
}

void TTerrain::set_mesh_lod_distance_ratio(real_t p_ratio) {
	ERR_FAIL_COND_EDMSG(p_ratio <= 0.0, "LOD level distance ratio must be positive.");
	data.lod_distance_ratio = p_ratio;
	// Update
}

real_t TTerrain::get_mesh_lod_distance_ratio() const {
	return data.lod_distance_ratio;
}

void TTerrain::set_mesh_lod_detail_levels(int p_levels) {
	mesh_lod_detail_levels = p_levels;
}

int TTerrain::get_mesh_lod_detail_levels() const {
	return mesh_lod_detail_levels;
}

void TTerrain::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			last_transform = get_global_transform();
			_enter_world();
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			// _exit_world();
		} break;
		case NOTIFICATION_ENTER_TREE: {
            // _update_visibility();
			// _enter_tree();
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			Transform3D new_xform = get_global_transform();
			if (new_xform == last_transform) {
				break;
			}

            last_transform = new_xform;
            // _update_transform();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
            // if (!is_inside_tree()) {
            //     break;
            // }
			// _update_visibility();
		} break;
		case NOTIFICATION_PROCESS: {
			if (update_view) {
				_update_view();
			}
		} break;
	}
}

void TTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &TTerrain::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &TTerrain::get_chunk_size);
	ClassDB::bind_method(D_METHOD("set_chunk_world_offset", "offset"), &TTerrain::set_chunk_world_offset);
	ClassDB::bind_method(D_METHOD("get_chunk_world_offset"), &TTerrain::get_chunk_world_offset);
	ClassDB::bind_method(D_METHOD("set_centered", "centered"), &TTerrain::set_centered);
	ClassDB::bind_method(D_METHOD("is_centered"), &TTerrain::is_centered);
	ClassDB::bind_method(D_METHOD("set_unit_size", "size"), &TTerrain::set_unit_size);
	ClassDB::bind_method(D_METHOD("get_unit_size"), &TTerrain::get_unit_size);
	ClassDB::bind_method(D_METHOD("set_chunk_manual_update", "manual"), &TTerrain::set_chunk_manual_update);
	ClassDB::bind_method(D_METHOD("is_chunk_manual_update"), &TTerrain::is_chunk_manual_update);
	ClassDB::bind_method(D_METHOD("set_chunk_active", "active"), &TTerrain::set_chunk_active);
	ClassDB::bind_method(D_METHOD("get_chunk_active"), &TTerrain::get_chunk_active);

	ClassDB::bind_method(D_METHOD("set_mesh_quad_tree_leaf_size", "size"), &TTerrain::set_mesh_quad_tree_leaf_size);
	ClassDB::bind_method(D_METHOD("get_mesh_quad_tree_leaf_size"), &TTerrain::get_mesh_quad_tree_leaf_size);
	ClassDB::bind_method(D_METHOD("set_mesh_render_resolution_multiplier", "multiplier"), &TTerrain::set_mesh_render_resolution_multiplier);
	ClassDB::bind_method(D_METHOD("get_mesh_render_resolution_multiplier"), &TTerrain::get_mesh_render_resolution_multiplier);
	ClassDB::bind_method(D_METHOD("set_mesh_lod_level_count", "count"), &TTerrain::set_mesh_lod_level_count);
	ClassDB::bind_method(D_METHOD("get_mesh_lod_level_count"), &TTerrain::get_mesh_lod_level_count);
	ClassDB::bind_method(D_METHOD("set_mesh_lod_distance_ratio", "ratio"), &TTerrain::set_mesh_lod_distance_ratio);
	ClassDB::bind_method(D_METHOD("get_mesh_lod_distance_ratio"), &TTerrain::get_mesh_lod_distance_ratio);
	ClassDB::bind_method(D_METHOD("set_mesh_lod_detail_levels", "levels"), &TTerrain::set_mesh_lod_detail_levels);
	ClassDB::bind_method(D_METHOD("get_mesh_lod_detail_levels"), &TTerrain::get_mesh_lod_detail_levels);

	ADD_GROUP("Chunk", "chunk_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size", PROPERTY_HINT_RANGE, "4,2048"), "set_chunk_size", "get_chunk_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "chunk_world_offset"), "set_chunk_world_offset", "get_chunk_world_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "centered"), "set_centered", "is_centered");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "quad_size", PROPERTY_HINT_RANGE, "0.1,10.0,0.1,or_greater"), "set_unit_size", "get_unit_size");
	ADD_SUBGROUP("Chunk Manual Update", "chunk_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "chunk_manual_update", PROPERTY_HINT_GROUP_ENABLE), "set_chunk_manual_update", "is_chunk_manual_update");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "chunk_active"), "set_chunk_active", "get_chunk_active");

	ADD_GROUP("Mesh", "mesh_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_quad_tree_leaf_size", PROPERTY_HINT_RANGE, "2,128"), "set_mesh_quad_tree_leaf_size", "get_mesh_quad_tree_leaf_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_render_resolution_multiplier", PROPERTY_HINT_RANGE, "1,16"), "set_mesh_render_resolution_multiplier", "get_mesh_render_resolution_multiplier");
	ADD_SUBGROUP("LOD", "mesh_lod_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_lod_level_count", PROPERTY_HINT_RANGE, "2,15"), "set_mesh_lod_level_count", "get_mesh_lod_level_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_lod_distance_ratio", PROPERTY_HINT_RANGE, "1.0,4.0,0.1"), "set_mesh_lod_distance_ratio", "get_mesh_lod_distance_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_lod_detail_levels", PROPERTY_HINT_RANGE, "1,10"), "set_mesh_lod_detail_levels", "get_mesh_lod_detail_levels");
}

void TTerrain::_enter_world() {
	// for (const KeyValue<GridKey, Section *> &kv : section_map) {
	// 	Section *const section = kv.value;
	// 	if (section->status & SECTION_STATUS_INSTANCED) {
	// 		const Vector3 offset = _section_key_to_local(kv.key - section_world_offset);
	// 		const Transform3D xform = get_global_transform().translated(offset);
	// 		if (section->status & SECTION_STATUS_PHYSICS_ACTIVE) {
	// 			_section_physics_enter_world(section, xform);
	// 		}
	// 		if (section->status & SECTION_STATUS_RENDER_ACTIVE) {
	// 			_section_render_enter_world(section, xform);
	// 		}
	// 	}
	// }

	if (!Engine::get_singleton()->is_editor_hint()) {
		_set_viewport_camera();
	}
}

void TTerrain::_set_viewport_camera() {
	Viewport *viewport = get_viewport();

	if (viewport) {
		set_camera(viewport->get_camera_3d());
	}
}

void TTerrain::_update_viewer() {
	if (!camera || !camera->is_inside_tree()) {
		if (!Engine::get_singleton()->is_editor_hint()) {
			_set_viewport_camera();
		}

		if (!camera || !camera->is_inside_tree()) {
			update_view = false;
			return;
		}
	}

	if (update_view) {
		quad_tree->view_range = camera->get_far();
		viewer_transform = camera->get_global_transform();
	} else if (quad_tree->view_range != camera->get_far()) {
		quad_tree->view_range = camera->get_far();
		viewer_transform = camera->get_global_transform();
		update_view = true;
	} else {
		Transform3D cam_xform = camera->get_global_transform();

		if (cam_xform.origin.distance_squared_to(viewer_transform.origin) > update_distance_tolerance_squared || !cam_xform.basis.get_euler().is_equal_approx(viewer_transform.basis.get_euler())) {
			viewer_transform = cam_xform;
			update_view = true;
		}
	}
}

void TTerrain::_update_view() {
	update_view = false;
}

void TTerrain::_check_mesh_render_resolution() {
	if (data.quad_tree_leaf_size * mesh_render_resolution_multiplier > 128) {
		mesh_render_resolution_multiplier = 128 / data.quad_tree_leaf_size;
		WARN_PRINT_ED("Mesh Render Resolution too high. Value has been adjusted.");
	}
}

TTerrain::TTerrain() {
	set_notify_transform(true);
	quad_tree.instantiate(data);
	set_process(false);
}

TTerrain::~TTerrain() {
}