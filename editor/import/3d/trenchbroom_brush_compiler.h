/**************************************************************************/
/*  trenchbroom_brush_compiler.h                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "editor/import/3d/trenchbroom_map_parser.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

class TrenchBroomBrushCompiler {
public:
	struct MaterialInfo {
		Ref<Material> material;
		Vector2 texture_size = Vector2(64, 64);
	};
	struct Options {
		real_t unit_scale = 1.0 / 32.0;
		real_t tolerance = 0.001;
		HashMap<String, MaterialInfo> materials;
	};
	struct Result {
		Ref<ArrayMesh> mesh;
		Ref<ConvexPolygonShape3D> collision;
	};
	// Plane point order must produce outward normals. Open, duplicate, redundant,
	// and degenerate planes are rejected; tolerance is measured in map units.
	// Each face becomes a named surface. Unresolved materials use 64x64 texel UVs.
	// Coordinates convert from map (X, Y, Z) to Godot (Y, Z, X), matching
	// existing Godot TrenchBroom projects.
	static Error compile(const TrenchBroomMapParser::Brush &p_brush, TrenchBroomMapParser::Format p_format, const Options &p_options, Result &r_result, TrenchBroomMapParser::Diagnostic &r_diagnostic);
};
