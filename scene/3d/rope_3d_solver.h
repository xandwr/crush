/**************************************************************************/
/*  rope_3d_solver.h                                                     */
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

#include "core/math/vector3.h"
#include "core/templates/vector.h"

// Internal world-space XPBD chain. Scene ownership and contacts belong to Rope3D.
class Rope3DSolver {
public:
	struct Particle {
		Vector3 position;
		Vector3 previous;
		Vector3 velocity;
		real_t inverse_mass = 1;
		bool attached = false;
		real_t compliance = 0;
		Vector3 target;
		Vector3 attachment_lambda;
		Vector3 pose_lambda;
	};
	struct Segment {
		real_t length = 0;
		Vector3 direction = Vector3(0, -1, 0);
		real_t lambda = 0;
	};
	Vector<Particle> particles;
	Vector<Segment> segments;
	Vector<real_t> rest_angles;
	Vector<real_t> bend_lambdas;
	Vector<Vector3> pose;
	Vector3 gravity = Vector3(0, -9.8, 0);
	real_t rest_length = 1;
	real_t mass = 1;
	real_t stretch_compliance = 0;
	real_t bend_compliance = 0.01;
	real_t damping = 2;
	real_t pose_compliance = 0;
	bool bend_enabled = false;

	static Vector<Vector3> resample(const Vector<Vector3> &p_points, int p_count);
	void reset(const Vector<Vector3> &p_points, bool p_capture_rest);
	void update_lengths();
	void predict(real_t p_delta);
	void project(real_t p_delta, bool p_reverse);
	void restore_pins();
	void finish(real_t p_delta);
	real_t weight(int p_index) const;
	real_t max_error() const;
	bool overstretched() const;
};
