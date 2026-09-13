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
#include "scene/3d/visual_instance_3d.h"
#include "scene/3d/viewmodel_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering/rendering_server.h"

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

TEST_CASE("[SceneTree][Viewmodel3D] Camera ownership") {
	Window *root = SceneTree::get_singleton()->get_root();
	Camera3D *first_camera = memnew(Camera3D);
	Camera3D *second_camera = memnew(Camera3D);
	Node3D *subtree = memnew(Node3D);
	Node3D *intermediate = memnew(Node3D);
	Viewmodel3D *viewmodel = memnew(Viewmodel3D);

	intermediate->add_child(viewmodel);
	subtree->add_child(intermediate);
	CHECK(viewmodel->get_camera_3d() == nullptr);

	root->add_child(first_camera);
	root->add_child(second_camera);
	first_camera->add_child(subtree);
	CHECK(viewmodel->get_camera_3d() == first_camera);

	subtree->reparent(second_camera);
	CHECK(viewmodel->get_camera_3d() == second_camera);

	second_camera->remove_child(subtree);
	CHECK(viewmodel->get_camera_3d() == nullptr);

	memdelete(subtree);
	memdelete(first_camera);
	memdelete(second_camera);
}

TEST_CASE("[SceneTree][Viewmodel3D] Nearest camera owns viewmodel") {
	Window *root = SceneTree::get_singleton()->get_root();
	Camera3D *outer_camera = memnew(Camera3D);
	Camera3D *inner_camera = memnew(Camera3D);
	Viewmodel3D *viewmodel = memnew(Viewmodel3D);

	inner_camera->add_child(viewmodel);
	outer_camera->add_child(inner_camera);
	root->add_child(outer_camera);
	CHECK(viewmodel->get_camera_3d() == inner_camera);

	memdelete(outer_camera);
}

TEST_CASE("[SceneTree][Viewmodel3D] Visual instance ownership") {
	Window *root = SceneTree::get_singleton()->get_root();
	Camera3D *first_camera = memnew(Camera3D);
	Camera3D *second_camera = memnew(Camera3D);
	Viewmodel3D *viewmodel = memnew(Viewmodel3D);
	Node3D *subtree = memnew(Node3D);
	VisualInstance3D *first_visual = memnew(VisualInstance3D);

	root->add_child(first_camera);
	root->add_child(second_camera);
	first_camera->add_child(viewmodel);
	viewmodel->add_child(subtree);
	subtree->add_child(first_visual);
	CHECK(RenderingServer::get_singleton()->camera_is_viewmodel_enabled(first_camera->get_camera()));
	CHECK_FALSE(RenderingServer::get_singleton()->camera_is_viewmodel_casting_world_shadows(first_camera->get_camera()));
	CHECK(RenderingServer::get_singleton()->instance_get_viewmodel_camera(first_visual->get_instance()) == first_camera->get_camera());
	viewmodel->set_cast_world_shadows(true);
	CHECK(RenderingServer::get_singleton()->camera_is_viewmodel_casting_world_shadows(first_camera->get_camera()));
	viewmodel->set_cast_world_shadows(false);

	viewmodel->set_enabled(false);
	CHECK_FALSE(RenderingServer::get_singleton()->camera_is_viewmodel_enabled(first_camera->get_camera()));
	CHECK(RenderingServer::get_singleton()->instance_get_viewmodel_camera(first_visual->get_instance()) == first_camera->get_camera());
	viewmodel->set_enabled(true);
	CHECK(RenderingServer::get_singleton()->camera_is_viewmodel_enabled(first_camera->get_camera()));

	VisualInstance3D *dynamic_visual = memnew(VisualInstance3D);
	subtree->add_child(dynamic_visual);
	CHECK(RenderingServer::get_singleton()->instance_get_viewmodel_camera(dynamic_visual->get_instance()) == first_camera->get_camera());

	viewmodel->reparent(second_camera);
	CHECK(RenderingServer::get_singleton()->instance_get_viewmodel_camera(first_visual->get_instance()) == second_camera->get_camera());
	CHECK(RenderingServer::get_singleton()->instance_get_viewmodel_camera(dynamic_visual->get_instance()) == second_camera->get_camera());

	second_camera->remove_child(viewmodel);
	CHECK_FALSE(RenderingServer::get_singleton()->instance_get_viewmodel_camera(first_visual->get_instance()).is_valid());
	CHECK_FALSE(RenderingServer::get_singleton()->instance_get_viewmodel_camera(dynamic_visual->get_instance()).is_valid());

	memdelete(viewmodel);
	memdelete(first_camera);
	memdelete(second_camera);
}

} // namespace TestViewmodel3D

#endif // _3D_DISABLED
