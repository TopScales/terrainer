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
	// If the mouse is currently captured, we are most likely in freelook mode.
	// In this case, disable shortcuts to avoid conflicts with freelook navigation.
	// if (!node || Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED) {
	// 	return EditorPlugin::AFTER_GUI_INPUT_PASS;
	// }

	// Ref<InputEventKey> k = p_event;
	// if (k.is_valid() && k->is_pressed() && !k->is_echo()) {
	// 	// Transform mode (toggle button):
	// 	// If we are in Transform mode we pass the events to the 3D editor,
	// 	// but if the Transform mode shortcut is pressed again, we go back to Selection mode.
	// 	if (mode_buttons_group->get_pressed_button() == transform_mode_button) {
	// 		if (transform_mode_button->get_shortcut().is_valid() && transform_mode_button->get_shortcut()->matches_event(p_event)) {
	// 			select_mode_button->set_pressed(true);
	// 			accept_event();
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		}
	// 		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	// 	}
	// 	// Tool modes and tool actions:
	// 	for (BaseButton *b : viewport_shortcut_buttons) {
	// 		if (b->is_disabled()) {
	// 			continue;
	// 		}

	// 		if (b->get_shortcut().is_valid() && b->get_shortcut()->matches_event(p_event)) {
	// 			if (b->is_toggle_mode()) {
	// 				b->set_pressed(b->get_button_group().is_valid() || !b->is_pressed());
	// 			} else {
	// 				// Can't press a button without toggle mode, so just emit the signal directly.
	// 				b->emit_signal(SceneStringName(pressed));
	// 			}
	// 			accept_event();
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		}
	// 	}
	// 	// Hard key actions:
	// 	if (k->get_keycode() == Key::ESCAPE) {
	// 		if (input_action == INPUT_PASTE) {
	// 			_clear_clipboard_data();
	// 			input_action = INPUT_NONE;
	// 			_update_paste_indicator();
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		} else if (selection.active) {
	// 			_set_selection(false);
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		} else {
	// 			input_action = INPUT_NONE;
	// 			update_palette();
	// 			_update_cursor_instance();
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		}
	// 	}
	// 	// Options menu shortcuts:
	// 	Ref<Shortcut> ed_shortcut = ED_GET_SHORTCUT("grid_map/previous_floor");
	// 	if (ed_shortcut.is_valid() && ed_shortcut->matches_event(p_event)) {
	// 		accept_event();
	// 		_menu_option(MENU_OPTION_PREV_LEVEL);
	// 		return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 	}
	// 	ed_shortcut = ED_GET_SHORTCUT("grid_map/next_floor");
	// 	if (ed_shortcut.is_valid() && ed_shortcut->matches_event(p_event)) {
	// 		accept_event();
	// 		_menu_option(MENU_OPTION_NEXT_LEVEL);
	// 		return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 	}
	// 	for (int i = 0; i < options->get_popup()->get_item_count(); ++i) {
	// 		const Ref<Shortcut> &shortcut = options->get_popup()->get_item_shortcut(i);
	// 		if (shortcut.is_valid() && shortcut->matches_event(p_event)) {
	// 			// Consume input to avoid conflicts with other plugins.
	// 			accept_event();
	// 			_menu_option(options->get_popup()->get_item_id(i));
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		}
	// 	}
	// }

	// Ref<InputEventMouseButton> mb = p_event;
	// if (mb.is_valid()) {
	// 	if (mb->get_button_index() == MouseButton::WHEEL_UP && (mb->is_command_or_control_pressed())) {
	// 		if (mb->is_pressed()) {
	// 			floor->set_value(floor->get_value() + mb->get_factor());
	// 		}

	// 		return EditorPlugin::AFTER_GUI_INPUT_STOP; // Eaten.
	// 	} else if (mb->get_button_index() == MouseButton::WHEEL_DOWN && (mb->is_command_or_control_pressed())) {
	// 		if (mb->is_pressed()) {
	// 			floor->set_value(floor->get_value() - mb->get_factor());
	// 		}
	// 		return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 	}

	// 	if (mb->is_pressed()) {
	// 		Node3DEditorViewport::NavigationScheme nav_scheme = (Node3DEditorViewport::NavigationScheme)EDITOR_GET("editors/3d/navigation/navigation_scheme").operator int();
	// 		if ((nav_scheme == Node3DEditorViewport::NAVIGATION_MAYA || nav_scheme == Node3DEditorViewport::NAVIGATION_MODO) && mb->is_alt_pressed()) {
	// 			input_action = INPUT_NONE;
	// 		} else if (mb->get_button_index() == MouseButton::LEFT) {
	// 			bool can_edit = (node && node->get_mesh_library().is_valid());
	// 			if (input_action == INPUT_PASTE) {
	// 				_do_paste();
	// 				input_action = INPUT_NONE;
	// 				_update_paste_indicator();
	// 				return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 			} else if (mode_buttons_group->get_pressed_button() == select_mode_button && can_edit) {
	// 				input_action = INPUT_SELECT;
	// 				last_selection = selection;
	// 			} else if (mode_buttons_group->get_pressed_button() == pick_mode_button && can_edit) {
	// 				input_action = INPUT_PICK;
	// 			} else if (mode_buttons_group->get_pressed_button() == paint_mode_button && can_edit) {
	// 				input_action = INPUT_PAINT;
	// 				set_items.clear();
	// 			} else if (mode_buttons_group->get_pressed_button() == erase_mode_button && can_edit) {
	// 				input_action = INPUT_ERASE;
	// 				set_items.clear();
	// 			}
	// 		} else if (mb->get_button_index() == MouseButton::RIGHT) {
	// 			if (input_action == INPUT_PASTE) {
	// 				_clear_clipboard_data();
	// 				input_action = INPUT_NONE;
	// 				_update_paste_indicator();
	// 				return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 			} else if (selection.active) {
	// 				_set_selection(false);
	// 				return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 			}
	// 		} else {
	// 			return EditorPlugin::AFTER_GUI_INPUT_PASS;
	// 		}

	// 		if (do_input_action(p_camera, Point2(mb->get_position().x, mb->get_position().y), true)) {
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		}
	// 		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	// 	} else {
	// 		if ((mb->get_button_index() == MouseButton::LEFT && input_action == INPUT_ERASE) || (mb->get_button_index() == MouseButton::LEFT && input_action == INPUT_PAINT)) {
	// 			if (set_items.size()) {
	// 				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	// 				undo_redo->create_action(TTR("GridMap Paint"));
	// 				for (const SetItem &si : set_items) {
	// 					undo_redo->add_do_method(node, "set_cell_item", si.position, si.new_value, si.new_orientation);
	// 				}
	// 				for (uint32_t i = set_items.size(); i > 0; i--) {
	// 					const SetItem &si = set_items[i - 1];
	// 					undo_redo->add_undo_method(node, "set_cell_item", si.position, si.old_value, si.old_orientation);
	// 				}

	// 				undo_redo->commit_action();
	// 			}
	// 			set_items.clear();
	// 			input_action = INPUT_NONE;

	// 			if (set_items.size() > 0) {
	// 				return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 			}
	// 			return EditorPlugin::AFTER_GUI_INPUT_PASS;
	// 		}

	// 		if (mb->get_button_index() == MouseButton::LEFT && input_action == INPUT_SELECT) {
	// 			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	// 			undo_redo->create_action(TTR("GridMap Selection"));
	// 			undo_redo->add_do_method(this, "_set_selection", selection.active, selection.begin, selection.end);
	// 			undo_redo->add_undo_method(this, "_set_selection", last_selection.active, last_selection.begin, last_selection.end);
	// 			undo_redo->commit_action();
	// 		}

	// 		if (mb->get_button_index() == MouseButton::LEFT && input_action != INPUT_NONE) {
	// 			set_items.clear();
	// 			input_action = INPUT_NONE;
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		}
	// 		if (mb->get_button_index() == MouseButton::RIGHT && (input_action == INPUT_ERASE || input_action == INPUT_PASTE)) {
	// 			input_action = INPUT_NONE;
	// 			return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 		}
	// 	}
	// }

	// Ref<InputEventMouseMotion> mm = p_event;

	// if (mm.is_valid()) {
	// 	// Update the grid, to check if the grid needs to be moved to a tile cursor.
	// 	update_grid();

	// 	if (do_input_action(p_camera, mm->get_position(), false)) {
	// 		return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 	}
	// 	return EditorPlugin::AFTER_GUI_INPUT_PASS;
	// }

	// Ref<InputEventPanGesture> pan_gesture = p_event;
	// if (pan_gesture.is_valid()) {
	// 	if (pan_gesture->is_alt_pressed() && pan_gesture->is_command_or_control_pressed()) {
	// 		const real_t delta = pan_gesture->get_delta().y * 0.5;
	// 		accumulated_floor_delta += delta;
	// 		int step = 0;
	// 		if (Math::abs(accumulated_floor_delta) > 1.0) {
	// 			step = SIGN(accumulated_floor_delta);
	// 			accumulated_floor_delta -= step;
	// 		}
	// 		if (step) {
	// 			floor->set_value(floor->get_value() + step);
	// 		}
	// 		return EditorPlugin::AFTER_GUI_INPUT_STOP;
	// 	}
	// }
	// accumulated_floor_delta = 0.0;

	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

void TerrainEditor::edit(TTerrain *p_terrain) {
	node = p_terrain;
}

void TerrainEditor::_notification(int p_what) {
	// switch(p_what) {
	// }
}

void TerrainEditor::_bind_methods() {
}

// ******** TerrainEditorPlugin ********

#ifdef TERRAINER_MODULE
EditorPlugin::AfterGUIInput TerrainEditorPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
#endif // TERRAINER_MODULE
#ifdef TERRAINER_GDEXTENSION
int32_t TerrainEditorPlugin::_forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
#endif // TERRAINER_GDEXTENSION
	for (TTerrain *terrain : nodes) {
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
	terrain_editor->edit(Object::cast_to<TTerrain>(p_object));
}

#ifdef TERRAINER_MODULE
bool TerrainEditorPlugin::handles(Object *p_object) const {
#endif // TERRAINER_MODULE
#ifdef TERRAINER_GDEXTENSION
bool TerrainEditorPlugin::_handles(Object *p_object) const {
#endif // TERRAINER_GDEXTENSION
	return p_object->is_class("TTerrain");
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

void TerrainEditorPlugin::_bind_methods() {

}

void TerrainEditorPlugin::_on_tree_node_added(Node *p_node) {
	TTerrain *terrain = Object::cast_to<TTerrain>(p_node);

	if (terrain && terrain->is_part_of_edited_scene()) {
		terrain->connect("tree_exited", callable_mp(this, &TerrainEditorPlugin::_on_terrain_exited).bind(terrain));
		nodes.push_back(terrain);
	}
}

void TerrainEditorPlugin::_on_terrain_exited(TTerrain *p_terrain) {
	p_terrain->disconnect("tree_exited", callable_mp(this, &TerrainEditorPlugin::_on_terrain_exited));
	nodes.erase(p_terrain);
}