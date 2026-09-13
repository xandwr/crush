/**************************************************************************/
/*  test_viewmodel_3d.cpp                                                 */
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

TEST_FORCE_LINK(test_viewmodel_3d)

#ifndef _3D_DISABLED

#include "core/object/class_db.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/viewmodel_3d.h"

namespace TestViewmodel3D {

TEST_CASE("[SceneTree][Viewmodel3D] Properties") {
	CHECK(ClassDB::class_exists("Viewmodel3D"));

	Viewmodel3D *viewmodel = memnew(Viewmodel3D);

	CHECK(viewmodel->is_enabled());
	CHECK(viewmodel->get_fov() == doctest::Approx(54.0));
	CHECK(viewmodel->get_near() == doctest::Approx(0.01));
	CHECK(viewmodel->get_far() == doctest::Approx(100.0));
	CHECK_FALSE(viewmodel->is_casting_world_shadows());

	viewmodel->set_enabled(false);
	viewmodel->set_fov(70.0);
	viewmodel->set_near(0.02);
	viewmodel->set_far(200.0);
	viewmodel->set_cast_world_shadows(true);

	CHECK_FALSE(viewmodel->is_enabled());
	CHECK(viewmodel->get_fov() == doctest::Approx(70.0));
	CHECK(viewmodel->get_near() == doctest::Approx(0.02));
	CHECK(viewmodel->get_far() == doctest::Approx(200.0));
	CHECK(viewmodel->is_casting_world_shadows());

	memdelete(viewmodel);
}

TEST_CASE("[SceneTree][Viewmodel3D] Camera ancestry warnings") {
	SUBCASE("No camera ancestor") {
		Viewmodel3D *viewmodel = memnew(Viewmodel3D);
		CHECK(viewmodel->get_configuration_warnings().size() == 1);
		memdelete(viewmodel);
	}

	SUBCASE("Indirect camera ancestor") {
		Camera3D *camera = memnew(Camera3D);
		Node3D *intermediate = memnew(Node3D);
		Viewmodel3D *viewmodel = memnew(Viewmodel3D);
		camera->add_child(intermediate);
		intermediate->add_child(viewmodel);
		CHECK(viewmodel->get_configuration_warnings().is_empty());
		memdelete(camera);
	}

	SUBCASE("Nested viewmodel") {
		Camera3D *camera = memnew(Camera3D);
		Viewmodel3D *outer = memnew(Viewmodel3D);
		Viewmodel3D *inner = memnew(Viewmodel3D);
		camera->add_child(outer);
		outer->add_child(inner);
		CHECK(outer->get_configuration_warnings().is_empty());
		CHECK(inner->get_configuration_warnings().size() == 1);
		memdelete(camera);
	}
}

} // namespace TestViewmodel3D

#endif // _3D_DISABLED
