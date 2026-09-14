/**************************************************************************/
/*  test_trenchbroom_map_parser.cpp                                       */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_trenchbroom_map_parser)

#ifdef TOOLS_ENABLED
#include "editor/import/3d/trenchbroom_map_parser.h"

namespace TestTrenchBroomMapParser {

using Parser = TrenchBroomMapParser;

static const char *standard_map = R"MAP(// Game: fixture
// Format: Standard
{
"classname" "worldspawn"
"message" "A \"quoted\" room"
{
( 0 0 0 ) ( 0 32 0 ) ( 32 32 0 ) stone/floor 8 -4 90 0.5 -2
( 0 0 32 ) ( 32 32 32 ) ( 0 32 32 ) stone/ceiling 0 0 0 1 1
( 0 0 0 ) ( 32 0 32 ) ( 0 0 32 ) stone/wall 0 0 0 1 1
( 0 32 0 ) ( 0 32 32 ) ( 32 32 32 ) stone/wall 0 0 0 1 1
( 0 0 0 ) ( 0 0 32 ) ( 0 32 32 ) stone/wall 0 0 0 1 1
( 32 0 0 ) ( 32 32 32 ) ( 32 0 32 ) stone/wall 0 0 0 1 1
}
}
{
"classname" "ent_spawnpoint"
"origin" "16 32 48"
"angle" "90"
"team" "2"
"custom" "C:\maps\test"
}
)MAP";

static const char *valve_face = "( 0 0 0 ) ( 0 32 0 ) ( 32 32 0 ) stone/floor [ 1 0 0 8 ] [ 0 -1 0 -4 ] 90 0.5 -2";

TEST_CASE("[Editor][TrenchBroomMapParser] Standard brush and entity fixture") {
	Parser::Map map;
	Parser::Diagnostic diagnostic;
	REQUIRE(Parser::parse(standard_map, Parser::STANDARD, map, diagnostic) == OK);
	CHECK(diagnostic.message.is_empty());
	REQUIRE(map.entities.size() == 2);
	CHECK(map.format == Parser::STANDARD);
	CHECK(map.entities[0].line == 3);
	CHECK(map.entities[0].properties["message"] == "A \"quoted\" room");
	REQUIRE(map.entities[0].brushes.size() == 1);
	const Parser::Brush &brush = map.entities[0].brushes[0];
	CHECK(brush.line == 6);
	REQUIRE(brush.faces.size() == 6);
	const Parser::Face &face = brush.faces[0];
	CHECK(face.line == 7);
	CHECK(face.points[1] == Vector3(0, 32, 0));
	CHECK(face.material == "stone/floor");
	CHECK(face.offset == Vector2(8, -4));
	CHECK(face.rotation == 90);
	CHECK(face.scale == Vector2(0.5, -2));
	CHECK(map.entities[1].brushes.is_empty());
	CHECK(map.entities[1].properties["classname"] == "ent_spawnpoint");
	CHECK(map.entities[1].properties["origin"] == "16 32 48");
	CHECK(map.entities[1].properties["team"] == "2");
	CHECK(map.entities[1].properties["custom"] == "C:\\maps\\test");
}

TEST_CASE("[Editor][TrenchBroomMapParser] Valve axes and whitespace") {
	Parser::Map map;
	Parser::Diagnostic diagnostic;
	String source = String::chr(0xfeff) + "{\r\n\"classname\" \"worldspawn\"\r\n{\r\n" + valve_face + "// face\r\n}\r\n}\r\n";
	REQUIRE(Parser::parse(source, Parser::VALVE_220, map, diagnostic) == OK);
	CHECK(map.format == Parser::VALVE_220);
	const Parser::Face &face = map.entities[0].brushes[0].faces[0];
	CHECK(face.line == 4);
	CHECK(face.u_axis == Vector3(1, 0, 0));
	CHECK(face.v_axis == Vector3(0, -1, 0));
	CHECK(face.offset == Vector2(8, -4));
	CHECK(face.scale == Vector2(0.5, -2));
	CHECK(face.rotation == 90);
	CHECK(Parser::parse(source, Parser::STANDARD, map, diagnostic) == ERR_PARSE_ERROR);
	CHECK(map.entities.is_empty());
	CHECK(Parser::parse(standard_map, Parser::VALVE_220, map, diagnostic) == ERR_PARSE_ERROR);
	CHECK(map.entities.is_empty());
}

TEST_CASE("[Editor][TrenchBroomMapParser] Malformed input is transactional") {
	const char *invalid[] = {
		"{\"classname\"}",
		"{\"classname\" \"unterminated}",
		"{\"classname\" \"worldspawn\"",
		"{\"team\" \"1\" \"team\" \"2\"}",
		"{\"\" \"value\"}",
		"{{}}",
		"{{brushDef {}}}",
		"{{( 0 0 0 ) ( 0 32 0 ) ( 32 32 0 ) stone 0 0 0 0 1}}",
		"{{( 0 0 0 ) ( 0 32 0 ) ( 32 32 0 ) stone 0 0 0 1 1 0 0 0}}",
		"{{( nan 0 0 ) ( 0 32 0 ) ( 32 32 0 ) stone 0 0 0 1 1}}",
		"{{( 1e999 0 0 ) ( 0 32 0 ) ( 32 32 0 ) stone 0 0 0 1 1}}",
		"{{( 0 0 0 ) ( 0 32 0 ) ( 32 32 0 ) \"\" 0 0 0 1 1}}",
		"{} trailing",
	};
	Parser::Map map;
	Parser::Diagnostic diagnostic;
	for (const char *source : invalid) {
		CAPTURE(source);
		REQUIRE(Parser::parse(standard_map, Parser::STANDARD, map, diagnostic) == OK);
		CHECK(Parser::parse(source, Parser::STANDARD, map, diagnostic) == ERR_PARSE_ERROR);
		CHECK(map.entities.is_empty());
		CHECK_FALSE(diagnostic.message.is_empty());
		CHECK(diagnostic.line >= 1);
		CHECK(diagnostic.column >= 1);
	}
}

TEST_CASE("[Editor][TrenchBroomMapParser] Error location and reuse") {
	Parser::Map map;
	Parser::Diagnostic diagnostic;
	CHECK(Parser::parse("// comment\n{\n  \"team\" 2\n}", Parser::STANDARD, map, diagnostic) == ERR_PARSE_ERROR);
	CHECK(diagnostic.line == 3);
	CHECK(diagnostic.column == 10);
	CHECK(Parser::parse("// empty map", Parser::STANDARD, map, diagnostic) == OK);
	CHECK(map.entities.is_empty());
	CHECK(diagnostic.message.is_empty());
	CHECK(diagnostic.line == 0);
	CHECK(Parser::parse("{}", static_cast<Parser::Format>(99), map, diagnostic) == ERR_INVALID_PARAMETER);
	CHECK_FALSE(diagnostic.message.is_empty());
}

TEST_CASE("[Editor][TrenchBroomMapParser] Numeric forms and multiple brushes") {
	Parser::Map map;
	Parser::Diagnostic diagnostic;
	String face = "( -1.25 +2 3e1 ) ( 0 32 0 ) ( 32 32 0 ) \"stone floor\" -8 4 1e2 -1 2";
	REQUIRE(Parser::parse("{{" + face + "}{" + face + "}\"classname\" \"func_detail\"}", Parser::STANDARD, map, diagnostic) == OK);
	REQUIRE(map.entities[0].brushes.size() == 2);
	const Parser::Face &parsed_face = map.entities[0].brushes[1].faces[0];
	CHECK(parsed_face.points[0] == Vector3(-1.25, 2, 30));
	CHECK(parsed_face.material == "stone floor");
	CHECK(parsed_face.rotation == 100);
}

} // namespace TestTrenchBroomMapParser
#endif // TOOLS_ENABLED
