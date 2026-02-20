/**
 * terrain_editor_plugin.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#include "terrain_editor_plugin.h"

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/classes/scene_tree.hpp>
#endif // TERRAINER_GDEXTENSION

EditorPlugin::AfterGUIInput TerrainEditor::forward_spatial_input_event(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

void TerrainEditor::edit(Terrain *p_terrain) {
	node = p_terrain;
}

// void TerrainEditor::_notification(int p_what) {
// 	// switch(p_what) {
// 	// }
// }

// void TerrainEditor::_bind_methods() {
// }

// ******** TerrainEditorPlugin ********

#ifdef TERRAINER_MODULE
EditorPlugin::AfterGUIInput TerrainEditorPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
#endif // TERRAINER_MODULE
#ifdef TERRAINER_GDEXTENSION
int32_t TerrainEditorPlugin::_forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
#endif // TERRAINER_GDEXTENSION
	for (Terrain *terrain : nodes) {
		terrain->set_camera(p_camera);
	}

	return terrain_editor->forward_spatial_input_event(p_camera, p_event);
}

#ifdef TERRAINER_MODULE
void TerrainEditorPlugin::edit(Object *p_object) {
#endif // TERRAINER_MODULE
#ifdef TERRAINER_GDEXTENSION
void TerrainEditorPlugin::_edit(Object *p_object) {
#endif // TERRAINER_GDEXTENSION
	ERR_FAIL_NULL(terrain_editor);
	terrain_editor->edit(Object::cast_to<Terrain>(p_object));
}

#ifdef TERRAINER_MODULE
bool TerrainEditorPlugin::handles(Object *p_object) const {
#endif // TERRAINER_MODULE
#ifdef TERRAINER_GDEXTENSION
bool TerrainEditorPlugin::_handles(Object *p_object) const {
#endif // TERRAINER_GDEXTENSION
	return p_object->is_class("Terrain");
}

#ifdef TERRAINER_MODULE
void TerrainEditorPlugin::make_visible(bool p_visible) {
#endif // TERRAINER_MODULE
#ifdef TERRAINER_GDEXTENSION
void TerrainEditorPlugin::_make_visible(bool p_visible) {
#endif // TERRAINER_GDEXTENSION
	ERR_FAIL_NULL(terrain_editor);
}

void TerrainEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			set_input_event_forwarding_always_enabled();
			terrain_editor = memnew(TerrainEditor);
			terrain_editor->hide();
			get_tree()->connect("node_added", callable_mp(this, &TerrainEditorPlugin::_on_tree_node_added));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (terrain_editor) {
				memdelete(terrain_editor);
			}

			terrain_editor = nullptr;
			panel_button = nullptr;
			get_tree()->disconnect("node_added", callable_mp(this, &TerrainEditorPlugin::_on_tree_node_added));
		} break;
	}
}

// void TerrainEditorPlugin::_bind_methods() {
// }

void TerrainEditorPlugin::_on_tree_node_added(Node *p_node) {
	Terrain *terrain = Object::cast_to<Terrain>(p_node);

	if (terrain && terrain->is_part_of_edited_scene()) {
		terrain->connect("tree_exited", callable_mp(this, &TerrainEditorPlugin::_on_terrain_exited).bind(terrain));
		nodes.push_back(terrain);
	}
}

void TerrainEditorPlugin::_on_terrain_exited(Terrain *p_terrain) {
	p_terrain->disconnect("tree_exited", callable_mp(this, &TerrainEditorPlugin::_on_terrain_exited));
	nodes.erase(p_terrain);
}