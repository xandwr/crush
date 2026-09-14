/**************************************************************************/
/*  test_rope_3d.cpp                                                     */
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

TEST_FORCE_LINK(test_rope_3d)

#ifndef _3D_DISABLED

#include "scene/3d/rope_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

namespace TestRope3D {

static Vector<Vector3> line(int p_count, real_t p_length = 1) {
	Vector<Vector3> points;
	for (int i = 0; i < p_count; i++) {
		points.push_back(Vector3(real_t(i) * p_length / (p_count - 1), 0, 0));
	}
	return points;
}

static void simulate(Rope3DSolver &r_solver, int p_rate, real_t p_duration, int p_iterations = 32) {
	for (int tick = 0; tick < int(p_duration * p_rate); tick++) {
		real_t dt = 1.0 / p_rate;
		r_solver.predict(dt);
		for (int i = 0; i < p_iterations; i++) {
			r_solver.project(dt, i % 2 != 0);
		}
		r_solver.finish(dt);
	}
}

TEST_CASE("[Rope3D] Arc length, mass, finite collapsed constraints") {
	Rope3DSolver solver;
	solver.reset(line(9), true);
	real_t mass = 0;
	for (const Rope3DSolver::Particle &particle : solver.particles) {
		mass += 1 / particle.inverse_mass;
	}
	CHECK(mass == doctest::Approx(1));
	CHECK(solver.particles[0].inverse_mass == doctest::Approx(16));
	CHECK(solver.particles[4].inverse_mass == doctest::Approx(8));
	Vector<Vector3> collapsed;
	collapsed.resize(9);
	solver.reset(collapsed, false);
	solver.bend_enabled = true;
	simulate(solver, 256, 1);
	for (const Rope3DSolver::Particle &particle : solver.particles) {
		CHECK(particle.position.is_finite());
		CHECK(particle.velocity.is_finite());
	}
	Vector<Vector3> uneven = { Vector3(), Vector3(0.1, 0, 0), Vector3(1, 0, 0) };
	Vector<Vector3> sampled = Rope3DSolver::resample(uneven, 5);
	CHECK(sampled[2].x == doctest::Approx(0.5));
}

TEST_CASE("[Rope3D] Hanging chain converges across rates and topology") {
	for (int count : { 5, 9, 17 }) {
		for (int rate : { 60, 120, 256 }) {
			Rope3DSolver solver;
			solver.reset(line(count), true);
			solver.damping = 8;
			solver.particles.write[0].attached = true;
			solver.particles.write[0].target = Vector3();
			simulate(solver, rate, 8, 64);
			CHECK(solver.rest_length == doctest::Approx(1));
			CHECK(solver.particles[0].position == Vector3());
			CHECK(solver.max_error() < 0.003);
			CHECK(solver.particles[count - 1].position.distance_to(Vector3(0, -1, 0)) < 0.035);
		}
	}
}

TEST_CASE("[Rope3D] Soft and intermediate pins, overstretch preserves anchors") {
	Rope3DSolver solver;
	solver.reset(line(9), true);
	solver.gravity = Vector3();
	solver.particles.write[0].attached = true;
	solver.particles.write[0].target = Vector3();
	solver.particles.write[4].attached = true;
	solver.particles.write[4].target = Vector3(0.5, 0, 0);
	solver.particles.write[8].attached = true;
	solver.particles.write[8].target = Vector3(2, 0, 0);
	simulate(solver, 120, 1);
	CHECK(solver.overstretched());
	CHECK(solver.rest_length == doctest::Approx(1));
	CHECK(solver.particles[4].position == Vector3(0.5, 0, 0));
	CHECK(solver.particles[8].position == Vector3(2, 0, 0));
	solver.particles.write[8].compliance = 0.01;
	simulate(solver, 120, 2);
	CHECK(solver.particles[8].position.x < 1.5);
	CHECK_FALSE(solver.overstretched());
}

TEST_CASE("[Rope3D] Loaded material response across rates and particle counts") {
	for (int count : { 5, 9, 17 }) {
		for (int rate : { 60, 120, 256 }) {
			Rope3DSolver solver;
			solver.reset(line(count), true);
			solver.damping = 8;
			solver.stretch_compliance = 0.01;
			solver.particles.write[0].attached = true;
			solver.particles.write[0].target = Vector3();
			simulate(solver, rate, 8, 64);
			// A uniform hanging rope integrates half its weight across whole-chain compliance.
			CHECK(solver.particles[count - 1].position.distance_to(Vector3(0, -1.049, 0)) < 0.006);
			solver.reset(line(count), true);
			solver.gravity = Vector3();
			solver.stretch_compliance = 0.001;
			solver.particles.write[count - 1].attached = true;
			solver.particles.write[count - 1].target = Vector3(2, 0, 0);
			solver.particles.write[count - 1].compliance = 0.01;
			simulate(solver, rate, 8, 64);
			CHECK(solver.particles[count - 1].position.distance_to(Vector3(1.090909, 0, 0)) < 0.04);
			simulate(solver, rate, 2, 256);
			CHECK(solver.particles[count - 1].position.distance_to(Vector3(1.090909, 0, 0)) < 0.012);
		}
	}
}

TEST_CASE("[Rope3D] Rest-angle bend returns to captured curve") {
	Rope3DSolver solver;
	Vector<Vector3> curve = { Vector3(), Vector3(0.5, 0, 0), Vector3(0.5, 0.5, 0) };
	solver.reset(curve, true);
	solver.gravity = Vector3();
	solver.bend_enabled = true;
	solver.bend_compliance = 0;
	solver.particles.write[0].attached = true;
	solver.particles.write[0].target = curve[0];
	solver.particles.write[1].attached = true;
	solver.particles.write[1].target = curve[1];
	solver.particles.write[2].position = Vector3(0.8, 0.4, 0);
	simulate(solver, 120, 1);
	CHECK(solver.particles[2].position.distance_to(curve[2]) < 0.001);
}

TEST_CASE("[Rope3D] Loaded bend response converges with density and tick rate") {
	for (int count : { 5, 9, 17 }) {
		for (int rate : { 60, 120, 256 }) {
			Rope3DSolver solver;
			solver.reset(line(count), true);
			solver.bend_enabled = true;
			solver.bend_compliance = 0.2;
			solver.gravity = Vector3(0, -0.98, 0);
			solver.damping = 8;
			solver.particles.write[0].attached = true;
			solver.particles.write[0].target = Vector3();
			solver.particles.write[1].attached = true;
			solver.particles.write[1].target = Vector3(1.0 / (count - 1), 0, 0);
			simulate(solver, rate, 4, 1024);
			// Discrete small-angle cantilever under uniform gravity, with the first edge clamped.
			real_t density_factor = real_t(count - 2) / (count - 1);
			real_t expected = 0.98 * 0.2 / 8 * density_factor * density_factor;
			CHECK(Math::abs(solver.particles[count - 1].position.y + expected) < 0.008);
			CHECK(solver.particles[count - 1].position.x > 0.99);
		}
	}
}

TEST_CASE("[SceneTree][Rope3D] Manual scheduling, reset, topology, posed isolation") {
	Rope3D *rope = memnew(Rope3D);
	SceneTree::get_singleton()->get_root()->add_child(rope);
	rope->set_simulation_process_mode(Rope3D::SIMULATION_PROCESS_MANUAL);
	rope->set_render_enabled(false);
	rope->set_gravity(Vector3());
	rope->reset_to_points(line(9));
	rope->set_particle_target(0, Vector3());
	rope->set_particle_target(8, Vector3(1, 0, 0));
	rope->apply_impulse(8, Vector3(0, 10, 0));
	rope->advance_simulation(0.1);
	CHECK(rope->get_point_position(8) == Vector3(1, 0, 0));
	CHECK(rope->get_dropped_simulation_time() == doctest::Approx(0.1 - 8.0 / 120));
	rope->advance_simulation(2);
	CHECK(rope->get_dropped_simulation_time() > 1.9);
	for (int i = 0; i < 4; i++) {
		rope->advance_simulation(real_t(1e38));
	}
	CHECK(Math::is_finite(rope->get_dropped_simulation_time()));
	CHECK(rope->get_dropped_simulation_time() > 1e38);
	rope->reset_to_points(line(9));
	CHECK(rope->get_dropped_simulation_time() == 0);
	rope->set_particle_count(17);
	CHECK(rope->get_simulated_points().size() == 17);
	rope->clear_attachment(8);
	rope->set_simulation_enabled(false);
	Vector<Vector3> before = rope->get_simulated_points();
	rope->set_render_points(line(17, 2));
	rope->advance_simulation(0.01);
	CHECK(rope->get_simulated_points() == before);
	ERR_PRINT_OFF;
	rope->set_particle_count(1);
	rope->set_rest_length(Math::NaN);
	rope->set_total_mass(-1);
	rope->set_pose_targets(line(3), 0);
	rope->reset_to_points(line(3));
	rope->set_particle_target(-1, Vector3());
	rope->set_render_points({ Vector3(), Vector3(Math::INF, 0, 0) });
	ERR_PRINT_ON;
	CHECK(rope->get_particle_count() == 17);
	CHECK(rope->get_rest_length() == 1);
	CHECK(rope->get_simulated_points() == before);
	memdelete(rope);
}

TEST_CASE("[SceneTree][Rope3D] Node attachment removal and parent motion") {
	Node3D *parent = memnew(Node3D);
	Node3D *anchor = memnew(Node3D);
	anchor->set_name("Anchor");
	Rope3D *rope = memnew(Rope3D);
	parent->add_child(anchor);
	parent->add_child(rope);
	SceneTree::get_singleton()->get_root()->add_child(parent);
	rope->set_simulation_process_mode(Rope3D::SIMULATION_PROCESS_MANUAL);
	rope->set_gravity(Vector3());
	anchor->set_position(Vector3(0, 2, 0));
	rope->attach_particle(0, NodePath("../Anchor"));
	rope->advance_simulation(0.01);
	CHECK(rope->get_point_position(0) == anchor->get_global_position());
	memdelete(anchor);
	CHECK(rope->get_configuration_warnings().size() >= 1);
	rope->advance_simulation(0.01);
	rope->clear_attachment(0);
	rope->set("attachments/4/enabled", true);
	CHECK(rope->get_configuration_warnings().size() >= 1);
	rope->advance_simulation(0.01);
	rope->clear_attachment(4);
	Vector<Vector3> before = rope->get_simulated_points();
	parent->set_position(Vector3(10, 0, 0));
	CHECK(rope->get_simulated_points() == before);
	parent->set_scale(Vector3(2, 3, 4));
	CHECK(rope->get_rest_length() == 1);
	CHECK(rope->get_simulated_points() == before);
	memdelete(parent);
}

TEST_CASE("[SceneTree][Rope3D] Persistent tube, bounds, winding, UV and caps") {
	Rope3D *rope = memnew(Rope3D);
	rope->set_simulation_enabled(false);
	SceneTree::get_singleton()->get_root()->add_child(rope);
	rope->set_radius(0.1);
	for (Rope3D::CapMode cap : { Rope3D::CAP_NONE, Rope3D::CAP_FLAT, Rope3D::CAP_ROUND }) {
		Vector<Vector3> points = line(9);
		rope->set_render_points(points);
		rope->set_cap_mode(cap);
		Ref<ArrayMesh> mesh = rope->get_mesh();
		Array arrays = mesh->surface_get_arrays(0);
		Vector<int> indices = arrays[Mesh::ARRAY_INDEX];
		Vector<Vector3> vertices = arrays[Mesh::ARRAY_VERTEX];
		Vector<Vector3> normals = arrays[Mesh::ARRAY_NORMAL];
		Vector<Vector2> uv = arrays[Mesh::ARRAY_TEX_UV];
		CHECK(mesh->get_surface_count() == 1);
		CHECK(uv[8 * 9].y == doctest::Approx(1));
		for (int i = 0; i < vertices.size(); i++) {
			CHECK(vertices[i].is_finite());
			CHECK(normals[i].length() == doctest::Approx(1).epsilon(0.001));
			CHECK(rope->get_aabb().grow(0.0001).has_point(vertices[i]));
		}
		for (int i = 0; i < indices.size(); i += 3) {
			Vector3 face = (vertices[indices[i + 1]] - vertices[indices[i]]).cross(vertices[indices[i + 2]] - vertices[indices[i]]);
			CHECK(face.dot(normals[indices[i]] + normals[indices[i + 1]] + normals[indices[i + 2]]) <= 0.00001);
		}
		points.write[4] += Vector3(0, 0.3, 0);
		rope->set_render_points(points);
		CHECK(rope->get_mesh() == mesh);
		CHECK(rope->get_mesh()->surface_get_array_len(0) == vertices.size());
		CHECK(Vector<int>(rope->get_mesh()->surface_get_arrays(0)[Mesh::ARRAY_INDEX]) == indices);
	}
	memdelete(rope);
}

} // namespace TestRope3D
#endif
