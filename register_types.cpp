/**
 * register_types.cpp
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef _3D_DISABLED

#include "register_types.h"

#include "terrain.h"

#ifdef TOOLS_ENABLED
#include "editor/terrain_editor_plugin.h"
#endif

#include "core/object/class_db.h"

void initialize_terrainer_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(TTerrain);
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(TerrainEditorPlugin);
		EditorPlugins::add_by_type<TerrainEditorPlugin>();
	}
#endif
}

void uninitialize_terrainer_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

#endif // _3D_DISABLED
