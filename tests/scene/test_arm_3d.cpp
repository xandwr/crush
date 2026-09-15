/**************************************************************************/
/*  test_arm_3d.cpp                                                      */
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

TEST_FORCE_LINK(test_arm_3d)

#ifndef _3D_DISABLED
#include "scene/3d/arm_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

namespace TestArm3D {

struct Fixture {
	Node3D *root = memnew(Node3D);
	Node3D *shoulder = memnew(Node3D);
	Node3D *hand = memnew(Node3D);
	Arm3D *arm = memnew(Arm3D);
	Fixture() {
		shoulder->set_name("Shoulder");
		hand->set_name("Hand");
		hand->set_position(Vector3(0, 0, -0.7));
		root->add_child(shoulder);
		root->add_child(hand);
		root->add_child(arm);
		arm->set_shoulder_target(NodePath("../Shoulder"));
		arm->set_hand_target(NodePath("../Hand"));
		SceneTree::get_singleton()->get_root()->add_child(root);
		arm->reset_simulation();
	}
	~Fixture() { memdelete(root); }
	void tick(real_t p_delta) { SceneTree::get_singleton()->physics_process(p_delta); }
};

TEST_CASE("[SceneTree][Arm3D] Two lengths, shoulder orientation and exact targets") {
	Fixture f;
	f.arm->set_upper_arm_length(0.5);
	f.arm->set_forearm_length(0.3);
	f.arm->set_simulation_enabled(false);
	Vector<Vector3> points = f.arm->get_joint_positions();
	REQUIRE(points.size() == 3);
	CHECK(points[0].distance_to(points[1]) == doctest::Approx(0.5));
	CHECK(points[1].distance_to(points[2]) == doctest::Approx(0.3));
	CHECK(points[1].y < 0);
	f.shoulder->set_rotation(Vector3(0, 0, Math::PI));
	f.arm->reset_simulation();
	CHECK(f.arm->get_joint_positions()[1].y > 0);
	f.shoulder->set_rotation(Vector3());
	f.arm->set_elbow_direction(Vector3(1, -1, 0));
	CHECK(f.arm->get_joint_positions()[1].x > 0);
	f.shoulder->set_scale(Vector3(-1, 1, 1));
	f.arm->reset_simulation();
	CHECK(f.arm->get_joint_positions()[1].x < 0);
	for (Vector3 target : { Vector3(0, 0, -2), Vector3(0, 0, -0.1), Vector3(), Vector3(0, -0.7, 0) }) {
		f.hand->set_position(target);
		f.arm->reset_simulation();
		points = f.arm->get_joint_positions();
		CHECK(points[0] == f.shoulder->get_global_position());
		CHECK(points[0].distance_to(points[1]) == doctest::Approx(0.5));
		CHECK(points[1].distance_to(points[2]) == doctest::Approx(0.3));
		CHECK(points[0].distance_to(points[2]) <= 0.80001);
		CHECK(f.hand->get_position() == target);
		CHECK(points[1].is_finite());
	}
}

TEST_CASE("[SceneTree][Arm3D] Unreachable targets preserve reach in preview and physics") {
	for (bool simulate : { false, true }) {
		for (real_t upper : { real_t(0.3), real_t(0.5) }) {
			Fixture f;
			f.arm->set_simulation_enabled(simulate);
			f.arm->set_upper_arm_length(upper);
			f.arm->set_forearm_length(0.5);
			for (Vector3 target : { Vector3(50, 20, -30), Vector3(0, 0, -0.01), Vector3() }) {
				f.hand->set_position(target);
				f.arm->reset_simulation();
				for (int i = 0; i < 10; i++) {
					f.tick(1.0 / 60);
				}
				Vector<Vector3> points = f.arm->get_joint_positions();
				CHECK(points[0] == f.shoulder->get_global_position());
				CHECK(points[0].distance_to(points[1]) == doctest::Approx(upper));
				CHECK(points[1].distance_to(points[2]) == doctest::Approx(0.5));
				CHECK(points[0].distance_to(points[2]) <= upper + 0.50001);
				CHECK(f.hand->get_position() == target);
			}
			f.hand->set_position(Vector3(0, 0, -0.6));
			f.tick(1.0 / 60);
			CHECK(f.arm->get_joint_positions()[2] == f.hand->get_global_position());
		}
	}
}

TEST_CASE("[SceneTree][Arm3D] Physics stays finite across rates and moving targets") {
	for (int rate : { 30, 60, 120, 256 }) {
		Fixture f;
		f.arm->set_upper_arm_length(0.5);
		f.arm->set_forearm_length(0.3);
		f.arm->set_elbow_direction(Vector3(1, -1, 0));
		f.arm->apply_elbow_impulse(Vector3(0.02, 0, 0));
		for (int frame = 0; frame < rate * 2; frame++) {
			f.hand->set_position(Vector3(Math::sin(real_t(frame) / rate) * 0.08, 0, -0.65));
			f.tick(1.0 / rate);
			Vector<Vector3> points = f.arm->get_joint_positions();
			CHECK(points[0] == f.shoulder->get_global_position());
			CHECK(points[2] == f.hand->get_global_position());
			CHECK(points[1].is_finite());
			CHECK(Math::abs(points[0].distance_to(points[1]) - 0.5) < 0.005);
			CHECK(Math::abs(points[1].distance_to(points[2]) - 0.3) < 0.005);
		}
		CHECK(f.arm->get_dropped_simulation_time() < 0.00001);
		f.tick(1);
		CHECK(f.arm->get_dropped_simulation_time() > 0.9);
		f.root->set_position(Vector3(100, 20, 0));
		f.arm->reset_simulation();
		CHECK(f.arm->get_joint_positions()[0] == f.shoulder->get_global_position());
		CHECK(f.arm->get_dropped_simulation_time() == 0);
	}
}

TEST_CASE("[SceneTree][Arm3D] Missing targets, replacement and validation") {
	Fixture f;
	f.arm->set_hand_target(NodePath("../Missing"));
	CHECK(f.arm->get_joint_positions().is_empty());
	CHECK_FALSE(f.arm->get_configuration_warnings().is_empty());
	f.arm->set_hand_target(NodePath("../Hand"));
	CHECK(f.arm->get_joint_positions().size() == 3);
	memdelete(f.hand);
	f.tick(1.0 / 60);
	CHECK(f.arm->get_joint_positions().is_empty());
	f.hand = memnew(Node3D);
	f.hand->set_name("Hand");
	f.hand->set_position(Vector3(0.1, 0, -0.5));
	f.root->add_child(f.hand);
	f.tick(1.0 / 60);
	CHECK(f.arm->get_joint_positions()[2] == f.hand->get_global_position());
	ERR_PRINT_OFF;
	f.arm->set_upper_arm_length(-1);
	f.arm->set_forearm_length(Math::NaN);
	f.arm->set_elbow_direction(Vector3());
	f.arm->set_radius(Math::INF);
	f.arm->set_total_mass(0);
	ERR_PRINT_ON;
	CHECK(f.arm->get_upper_arm_length() == doctest::Approx(0.46));
	CHECK(f.arm->get_forearm_length() == doctest::Approx(0.45));
	CHECK(f.arm->get_elbow_direction() == Vector3(0, -1, 0));
	CHECK(f.arm->get_radius() == doctest::Approx(0.065));
	CHECK(f.arm->get_total_mass() == 1);
}

TEST_CASE("[SceneTree][Arm3D] Roundness only changes mesh, stable topology and bounds") {
	Fixture f;
	Vector<Vector3> joints = f.arm->get_joint_positions();
	f.arm->set_elbow_roundness(0.3);
	CHECK(f.arm->get_joint_positions() == joints);
	Ref<ArrayMesh> mesh = f.arm->get_mesh();
	REQUIRE(mesh->get_surface_count() == 1);
	Array first = mesh->surface_get_arrays(0);
	f.arm->set_elbow_roundness(0.7);
	CHECK(f.arm->get_joint_positions() == joints);
	CHECK(f.arm->get_mesh() == mesh);
	CHECK(Vector<int>(mesh->surface_get_arrays(0)[Mesh::ARRAY_INDEX]) == Vector<int>(first[Mesh::ARRAY_INDEX]));
	for (Vector3 scale : { Vector3(1, 1, 1), Vector3(-1, 2, 0.5) }) {
		f.arm->set_scale(scale);
		f.arm->reset_simulation();
		Array arrays = mesh->surface_get_arrays(0);
		Vector<Vector3> vertices = arrays[Mesh::ARRAY_VERTEX];
		Vector<Vector3> normals = arrays[Mesh::ARRAY_NORMAL];
		for (int i = 0; i < vertices.size(); i++) {
			CHECK(vertices[i].is_finite());
			CHECK(normals[i].is_finite());
			CHECK(f.arm->get_aabb().grow(0.0001).has_point(vertices[i]));
		}
	}
}

TEST_CASE("[SceneTree][Arm3D] Independent radii preserve joints and cap sizes") {
	Fixture f;
	Vector<Vector3> joints = f.arm->get_joint_positions();
	f.arm->set_upper_arm_radius(0.12);
	f.arm->set_forearm_radius(0.035);
	f.arm->set_upper_arm_end_radius(0.08);
	f.arm->set_forearm_end_radius(0.02);
	CHECK(f.arm->get_joint_positions() == joints);
	for (real_t roundness : { real_t(0), real_t(0.6) }) {
		f.arm->set_elbow_roundness(roundness);
		f.arm->set_radial_segments(roundness == 0 ? 13 : 12);
		Vector<Vector3> centerline = f.arm->get_render_points();
		Array arrays = f.arm->get_mesh()->surface_get_arrays(0);
		Vector<Vector3> vertices = arrays[Mesh::ARRAY_VERTEX];
		Vector<Vector3> normals = arrays[Mesh::ARRAY_NORMAL];
		int stride = f.arm->get_radial_segments() + 1;
		for (int side = 0; side < stride; side++) {
			CHECK(vertices[side].distance_to(centerline[0]) == doctest::Approx(0.12));
			CHECK(vertices[(centerline.size() - 1) * stride + side].distance_to(centerline[centerline.size() - 1]) == doctest::Approx(0.02));
			real_t entry_radius = roundness == 0 ? 0.084 : 0.092;
			real_t exit_radius = roundness == 0 ? 0.0335 : 0.0305;
			CHECK(vertices[stride + side].distance_to(centerline[1]) == doctest::Approx(entry_radius));
			CHECK(vertices[(centerline.size() - 2) * stride + side].distance_to(centerline[centerline.size() - 2]) == doctest::Approx(exit_radius));
		}
		for (int i = 0; i < vertices.size(); i++) {
			CHECK(vertices[i].is_finite());
			CHECK(normals[i].length() == doctest::Approx(1).epsilon(0.001));
			CHECK(f.arm->get_aabb().grow(0.0001).has_point(vertices[i]));
		}
		Vector<int> indices = arrays[Mesh::ARRAY_INDEX];
		f.arm->set_forearm_radius(0.04);
		CHECK(Vector<int>(f.arm->get_mesh()->surface_get_arrays(0)[Mesh::ARRAY_INDEX]) == indices);
		CHECK(f.arm->get_joint_positions() == joints);
		f.arm->set_forearm_radius(0.035);
	}
	f.arm->set("radius", 0.09);
	CHECK(f.arm->get_upper_arm_radius() == doctest::Approx(0.09));
	CHECK(f.arm->get_forearm_radius() == doctest::Approx(0.09));
	CHECK(f.arm->get_upper_arm_end_radius() == doctest::Approx(0.09));
	CHECK(f.arm->get_forearm_end_radius() == doctest::Approx(0.09));
	ERR_PRINT_OFF;
	f.arm->set_upper_arm_radius(-1);
	f.arm->set_forearm_radius(Math::NaN);
	f.arm->set_upper_arm_end_radius(0);
	f.arm->set_forearm_end_radius(Math::INF);
	ERR_PRINT_ON;
	CHECK(f.arm->get_upper_arm_radius() == doctest::Approx(0.09));
	CHECK(f.arm->get_forearm_radius() == doctest::Approx(0.09));
}

TEST_CASE("[SceneTree][Arm3D] Elbow impulse, stiffness and reset") {
	Fixture f;
	f.arm->set_gravity(Vector3());
	f.arm->set_elbow_stiffness(0);
	f.arm->set_damping(0);
	Vector3 rest = f.arm->get_joint_positions()[1];
	f.arm->apply_elbow_impulse(Vector3(0.2, 0, 0));
	for (int i = 0; i < 30; i++) {
		f.tick(1.0 / 120);
	}
	CHECK(f.arm->get_joint_positions()[1].distance_to(rest) > 0.03);
	f.arm->set_elbow_stiffness(10000);
	f.arm->set_damping(20);
	for (int i = 0; i < 240; i++) {
		f.tick(1.0 / 120);
	}
	CHECK(f.arm->get_joint_positions()[1].distance_to(rest) < 0.005);
	f.arm->apply_elbow_impulse(Vector3(1, 0, 0));
	f.arm->reset_simulation();
	f.tick(1.0 / 120);
	CHECK(f.arm->get_joint_positions()[1].distance_to(rest) < 0.0001);
}

TEST_CASE("[SceneTree][Arm3D] Scene serialization and focused properties") {
	Fixture f;
	f.shoulder->set_owner(f.root);
	f.hand->set_owner(f.root);
	f.arm->set_owner(f.root);
	f.arm->set_name("Arm");
	f.arm->set_upper_arm_length(0.6);
	f.arm->set_elbow_roundness(0.4);
	f.arm->set_upper_arm_radius(0.1);
	f.arm->set_forearm_radius(0.04);
	f.arm->set_upper_arm_end_radius(0.07);
	f.arm->set_forearm_end_radius(0.02);
	Ref<PackedScene> packed;
	packed.instantiate();
	CHECK(packed->pack(f.root) == OK);
	Node *copy = packed->instantiate();
	SceneTree::get_singleton()->get_root()->add_child(copy);
	Arm3D *arm = Object::cast_to<Arm3D>(copy->get_node(NodePath("Arm")));
	REQUIRE(arm);
	arm->reset_simulation();
	CHECK(arm->get_upper_arm_length() == doctest::Approx(0.6));
	CHECK(arm->get_elbow_roundness() == doctest::Approx(0.4));
	CHECK(arm->get_upper_arm_radius() == doctest::Approx(0.1));
	CHECK(arm->get_forearm_radius() == doctest::Approx(0.04));
	CHECK(arm->get_upper_arm_end_radius() == doctest::Approx(0.07));
	CHECK(arm->get_forearm_end_radius() == doctest::Approx(0.02));
	CHECK(arm->get_joint_positions().size() == 3);
	List<PropertyInfo> properties;
	arm->get_property_list(&properties);
	for (const PropertyInfo &property : properties) {
		if (property.name == StringName("radius")) {
			CHECK((property.usage & (PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE)) == 0);
		}
		CHECK_FALSE(String(property.name).begins_with("attachments/"));
		CHECK(property.name != StringName("initial_curve"));
		CHECK(property.name != StringName("particle_count"));
	}
	memdelete(copy);
}

} // namespace TestArm3D
#endif
