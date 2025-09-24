/**
 * register_types.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_REGISTER_TYPES_H
#define TERRAINER_REGISTER_TYPES_H

#ifdef TERRAINER_MODULE
#include "modules/register_module_types.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/core/class_db.hpp>
using namespace godot;
#endif // TERRAINER_GDEXTENSION

#include "modules/register_module_types.h"

void initialize_terrainer_module(ModuleInitializationLevel p_level);
void uninitialize_terrainer_module(ModuleInitializationLevel p_level);

#endif // TERRAINER_REGISTER_TYPES_H