/**
 * terrain_editor_plugin.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_TERRAIN_EDITOR_PLUGIN_H
#define TERRAINER_TERRAIN_EDITOR_PLUGIN_H

#include "../terrain.h"

#ifdef TERRAINER_MODULE
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#endif // TERRAINER_GDEXTENSION

using namespace Terrainer;

// class TerrainEditor : public VBoxContainer {
//     GDCLASS(TerrainEditor, VBoxContainer);

// private:
//     Terrain *node = nullptr;

// protected:
//     void _notification(int p_what);
//     static void _bind_methods();

// public:
//     EditorPlugin::AfterGUIInput forward_spatial_input_event(Camera3D *p_camera, const Ref<InputEvent> &p_event);
//     void edit(Terrain *p_terrain);
// };

class TerrainEditorPlugin : public EditorPlugin {
	GDCLASS(TerrainEditorPlugin, EditorPlugin);

private:
	// TerrainEditor *terrain_editor = nullptr;
	// Button *panel_button = nullptr;
    // Vector<Terrain *> nodes;

    // void _on_tree_node_added(Node *p_node);
    // void _on_terrain_exited(Terrain *p_terrain);

protected:
	// void _notification(int p_what);
	// static void _bind_methods();

public:
#ifdef TERRAINER_MODULE
	// virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;
	virtual String get_plugin_name() const override { return "Terrain"; }
	bool has_main_screen() const override { return false; }
	// virtual void edit(Object *p_object) override;
	// virtual bool handles(Object *p_object) const override;
	// virtual void make_visible(bool p_visible) override;
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
	virtual int32_t _forward_3d_gui_input(Camera3D *p_viewport_camera, const Ref<InputEvent> &p_event) override;
	virtual String _get_plugin_name() const override { return "Terrain"; }
	bool _has_main_screen() const override { return false; }
	virtual void _edit(Object *p_object) override;
	virtual bool _handles(Object *p_object) const override;
	virtual void _make_visible(bool p_visible) override;
#endif // TERRAINER_GDEXTENSION
};

#endif // TERRAINER_TERRAIN_EDITOR_PLUGIN_H