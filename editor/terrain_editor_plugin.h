/**
 * terrain_editor_plugin.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_TERRAIN_EDITOR_PLUGIN_H
#define TERRAINER_TERRAIN_EDITOR_PLUGIN_H

#include "../terrain.h"

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"

class TerrainEditor : public VBoxContainer {
    GDCLASS(TerrainEditor, VBoxContainer);

private:
    TTerrain *node = nullptr;

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    EditorPlugin::AfterGUIInput forward_spatial_input_event(Camera3D *p_camera, const Ref<InputEvent> &p_event);
    void edit(TTerrain *p_terrain);
};

class TerrainEditorPlugin : public EditorPlugin {
	GDCLASS(TerrainEditorPlugin, EditorPlugin);

private:
	TerrainEditor *terrain_editor = nullptr;
	Button *panel_button = nullptr;
    Vector<TTerrain *> nodes;

    void _on_tree_node_added(Node *p_node);
    void _on_terrain_exited(TTerrain *p_terrain);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;
	virtual String get_plugin_name() const override { return "Terrain"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;
};

#endif // TERRAINER_TERRAIN_EDITOR_PLUGIN_H