/**************************************************************************/
/*  test_mirror_3d.cpp                                                    */
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

TEST_FORCE_LINK(test_mirror_3d)

#ifndef _3D_DISABLED
#include "core/object/class_db.h"
#include "scene/3d/mirror_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

namespace TestMirror3D {

TEST_CASE("[SceneTree][Mirror3D] Validated properties and serialization") {
	CHECK(ClassDB::class_exists("Mirror3D"));
	Mirror3D *mirror = memnew(Mirror3D);
	CHECK(mirror->is_enabled());
	CHECK(mirror->get_size() == Vector2(2, 2));
	CHECK(mirror->get_resolution_scale() == doctest::Approx(0.5));
	CHECK(mirror->get_cull_mask() == 0xFFFFF);
	mirror->set_size(Vector2(4, 3));
	mirror->set_resolution_scale(0.75);
	mirror->set_cull_mask(7);
	mirror->set_enabled(false);
	ERR_PRINT_OFF;
	mirror->set_size(Vector2(0, 1));
	mirror->set_size(Vector2(Math::NaN, 1));
	mirror->set_resolution_scale(Math::INF);
	mirror->set_resolution_scale(0);
	mirror->set_resolution_scale(1.01);
	ERR_PRINT_ON;
	CHECK(mirror->get_size() == Vector2(4, 3));
	CHECK(mirror->get_resolution_scale() == doctest::Approx(0.75));
	CHECK(mirror->get_aabb() == AABB(Vector3(-2, -1.5, 0), Vector3(4, 3, 0)));
	Ref<PackedScene> packed;
	packed.instantiate();
	CHECK(packed->pack(mirror) == OK);
	Mirror3D *restored = Object::cast_to<Mirror3D>(packed->instantiate());
	REQUIRE(restored);
	CHECK(restored->get_size() == Vector2(4, 3));
	CHECK(restored->get_resolution_scale() == doctest::Approx(0.75));
	CHECK(restored->get_cull_mask() == 7);
	CHECK_FALSE(restored->is_enabled());
	CHECK(restored->get_child_count(true) == 0);
	memdelete(restored);
	memdelete(mirror);
}

TEST_CASE("[SceneTree][Mirror3D] Transform warnings and world lifecycle") {
	Mirror3D *mirror = memnew(Mirror3D);
	CHECK(mirror->get_configuration_warnings().is_empty());
	mirror->set_transform(Transform3D(Basis::from_scale(Vector3(2, 3, 1))));
	CHECK(mirror->get_configuration_warnings().is_empty());
	mirror->set_transform(Transform3D(Basis::from_scale(Vector3(-1, 1, 1))));
	CHECK(mirror->get_configuration_warnings().size() == 1);
	Basis shear;
	shear.set_column(1, Vector3(1, 1, 0));
	mirror->set_transform(Transform3D(shear));
	CHECK(mirror->get_configuration_warnings().size() == 1);
	mirror->set_transform(Transform3D());
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(mirror);
	CHECK(mirror->is_inside_tree());
	CHECK(mirror->get_child_count(true) == 0);
	mirror->hide();
	mirror->set_enabled(false);
	root->remove_child(mirror);
	root->add_child(mirror);
	mirror->show();
	mirror->set_enabled(true);
	CHECK(mirror->get_configuration_warnings().is_empty());
	root->remove_child(mirror);
	memdelete(mirror);
}

} //namespace TestMirror3D
#endif
