/**************************************************************************/
/*  trenchbroom_map_parser.cpp                                            */
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

#include "trenchbroom_map_parser.h"

#include "core/math/math_funcs.h"

#include <locale>
#include <sstream>

namespace {

class MapReader {
	const String &source;
	TrenchBroomMapParser::Format format;
	TrenchBroomMapParser::Diagnostic &diagnostic;
	int position = 0;
	int line = 1;
	int column = 1;
	String token;
	char32_t kind = 0;
	int token_line = 1;
	int token_column = 1;

	void advance() {
		if (source[position++] == '\n') {
			line++;
			column = 1;
		} else {
			column++;
		}
	}

	bool fail(const String &p_message) {
		if (diagnostic.message.is_empty()) {
			diagnostic.message = p_message;
			diagnostic.line = token_line;
			diagnostic.column = token_column;
		}
		return false;
	}

	bool next() {
		while (position < source.length()) {
			char32_t ch = source[position];
			if (ch <= ' ' || (position == 0 && ch == 0xfeff)) {
				advance();
			} else if (ch == '/' && position + 1 < source.length() && source[position + 1] == '/') {
				while (position < source.length() && source[position] != '\n') {
					advance();
				}
			} else {
				break;
			}
		}
		token_line = line;
		token_column = column;
		token = String();
		kind = 0;
		if (position == source.length()) {
			return true;
		}
		char32_t ch = source[position];
		if (ch == '{' || ch == '}' || ch == '(' || ch == ')' || ch == '[' || ch == ']') {
			kind = ch;
			advance();
			return true;
		}
		if (ch == '"') {
			kind = '"';
			advance();
			while (position < source.length()) {
				ch = source[position];
				if (ch == '"') {
					advance();
					return true;
				}
				if (ch == '\n' || ch == '\r') {
					return fail("Unterminated quoted string.");
				}
				if (ch == '\\' && position + 1 < source.length() && (source[position + 1] == '"' || source[position + 1] == '\\')) {
					advance();
					ch = source[position];
				}
				token += String::chr(ch);
				advance();
			}
			return fail("Unterminated quoted string.");
		}
		kind = 'a';
		int start = position;
		while (position < source.length()) {
			ch = source[position];
			if (ch <= ' ' || ch == '{' || ch == '}' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '"' || (ch == '/' && position + 1 < source.length() && source[position + 1] == '/')) {
				break;
			}
			advance();
		}
		token = source.substr(start, position - start);
		return true;
	}

	bool expect(char32_t p_kind) {
		if (kind != p_kind) {
			return fail("Expected '" + String::chr(p_kind) + "'.");
		}
		return next();
	}

	bool number(double &r_value, bool p_nonzero = false) {
		if (kind != 'a' || !token.is_valid_float()) {
			return fail("Expected a finite number.");
		}
		std::istringstream numeric_stream(token.ascii().get_data());
		numeric_stream.imbue(std::locale::classic());
		numeric_stream >> r_value;
		if (numeric_stream.fail() || !numeric_stream.eof()) {
			return fail("Number is outside the finite coordinate range.");
		}
		if (!Math::is_finite(r_value) || !Math::is_finite((real_t)r_value)) {
			return fail("Number is outside the finite coordinate range.");
		}
		if (p_nonzero && (r_value == 0 || (real_t)r_value == 0)) {
			return fail("Texture scales must be nonzero.");
		}
		return next();
	}

	bool vector(Vector3 &r_vector) {
		double values[3];
		for (int i = 0; i < 3; i++) {
			if (!number(values[i])) {
				return false;
			}
		}
		r_vector = Vector3(values[0], values[1], values[2]);
		return true;
	}

	bool face(TrenchBroomMapParser::Face &r_face) {
		r_face.line = token_line;
		for (int i = 0; i < 3; i++) {
			if (!expect('(') || !vector(r_face.points[i]) || !expect(')')) {
				return false;
			}
		}
		if ((kind != 'a' && kind != '"') || token.is_empty()) {
			return fail("Expected a material name.");
		}
		r_face.material = token;
		if (!next()) {
			return false;
		}
		double u_offset;
		double v_offset;
		if (format == TrenchBroomMapParser::VALVE_220) {
			if (!expect('[') || !vector(r_face.u_axis) || !number(u_offset) || !expect(']') || !expect('[') || !vector(r_face.v_axis) || !number(v_offset) || !expect(']')) {
				return false;
			}
		} else if (!number(u_offset) || !number(v_offset)) {
			return false;
		}
		double u_scale;
		double v_scale;
		if (!number(r_face.rotation) || !number(u_scale, true) || !number(v_scale, true)) {
			return false;
		}
		r_face.offset = Vector2(u_offset, v_offset);
		r_face.scale = Vector2(u_scale, v_scale);
		return true;
	}

	bool brush(TrenchBroomMapParser::Brush &r_brush) {
		r_brush.line = token_line;
		if (!expect('{')) {
			return false;
		}
		while (kind == '(') {
			TrenchBroomMapParser::Face parsed_face;
			if (!face(parsed_face)) {
				return false;
			}
			r_brush.faces.push_back(parsed_face);
		}
		if (kind != '}') {
			return fail("Expected a brush face or '}'. Unsupported brush syntax or face attributes.");
		}
		if (r_brush.faces.is_empty()) {
			return fail("Empty brush.");
		}
		return next();
	}

public:
	MapReader(const String &p_source, TrenchBroomMapParser::Format p_format, TrenchBroomMapParser::Diagnostic &r_diagnostic) :
			source(p_source), format(p_format), diagnostic(r_diagnostic) {}

	bool read(TrenchBroomMapParser::Map &r_map) {
		if (!next()) {
			return false;
		}
		while (kind != 0) {
			TrenchBroomMapParser::Entity entity;
			entity.line = token_line;
			if (!expect('{')) {
				return false;
			}
			while (kind == '"' || kind == '{') {
				if (kind == '{') {
					TrenchBroomMapParser::Brush parsed_brush;
					if (!brush(parsed_brush)) {
						return false;
					}
					entity.brushes.push_back(parsed_brush);
				} else {
					String key = token;
					if (key.is_empty() || entity.properties.has(key)) {
						return fail("Empty or duplicate entity property key.");
					}
					if (!next()) {
						return false;
					}
					if (kind != '"') {
						return fail("Expected a quoted entity property value.");
					}
					entity.properties.insert(key, token);
					if (!next()) {
						return false;
					}
				}
			}
			if (!expect('}')) {
				return false;
			}
			r_map.entities.push_back(entity);
		}
		return true;
	}
};

} // namespace

Error TrenchBroomMapParser::parse(const String &p_source, Format p_format, Map &r_map, Diagnostic &r_diagnostic) {
	r_map = Map();
	r_diagnostic = Diagnostic();
	if (p_format != STANDARD && p_format != VALVE_220) {
		r_diagnostic.message = "Unsupported map format.";
		return ERR_INVALID_PARAMETER;
	}
	Map parsed_map;
	parsed_map.format = p_format;
	MapReader reader(p_source, p_format, r_diagnostic);
	if (!reader.read(parsed_map)) {
		return ERR_PARSE_ERROR;
	}
	r_map = parsed_map;
	return OK;
}
