/**************************************************************************/
/*  trenchbroom_map_parser.h                                              */
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

#include "core/error/error_list.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

class TrenchBroomMapParser {
public:
	enum Format {
		STANDARD,
		VALVE_220,
	};

	// Coordinates stay in map units. The compiler validates and constructs brush geometry.
	struct Face {
		Vector3 points[3];
		String material;
		Vector3 u_axis;
		Vector3 v_axis;
		Vector2 offset;
		Vector2 scale;
		double rotation = 0;
		int line = 0;
	};

	struct Brush {
		Vector<Face> faces;
		int line = 0;
	};

	struct Entity {
		HashMap<String, String> properties;
		Vector<Brush> brushes;
		int line = 0;
	};

	struct Map {
		Format format = STANDARD;
		Vector<Entity> entities;
	};

	struct Diagnostic {
		String message;
		int line = 0;
		int column = 0;
	};

	// Failure clears the output map; syntax errors identify the offending token.
	static Error parse(const String &p_source, Format p_format, Map &r_map, Diagnostic &r_diagnostic);
};
