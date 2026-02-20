/**
 * register_types.cpp
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef _3D_DISABLED

#include "register_types.h"

#include "terrain.h"
#include "map_storage/map_storage.h"

#ifdef TOOLS_ENABLED
// #include "editor/terrain_editor_plugin.h"
#endif

#ifdef TERRAINER_MODULE
#include "core/object/class_db.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;
#endif // TERRAINER_GDEXTENSION

using namespace Terrainer;

void initialize_terrainer_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(Terrain);
		GDREGISTER_CLASS(MapStorage);
	}
#ifdef TOOLS_ENABLED
	// if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
	// 	GDREGISTER_CLASS(TerrainEditor);
	// 	GDREGISTER_CLASS(TerrainEditorPlugin);
	// 	EditorPlugins::add_by_type<TerrainEditorPlugin>();
	// }
#endif
}

void uninitialize_terrainer_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

#ifdef TERRAINER_GDEXTENSION
extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT terrainer_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_terrainer_module);
	init_obj.register_terminator(uninitialize_terrainer_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
#endif // TERRAINER_GDEXTENSION

#endif // _3D_DISABLED
