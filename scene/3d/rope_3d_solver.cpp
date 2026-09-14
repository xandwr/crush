/**************************************************************************/
/*  rope_3d_solver.cpp                                                   */
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

#include "rope_3d_solver.h"

Vector<Vector3> Rope3DSolver::resample(const Vector<Vector3> &p_points, int p_count) {
	Vector<real_t> lengths;
	lengths.resize(p_points.size());
	lengths.write[0] = 0;
	for (int i = 1; i < p_points.size(); i++) {
		lengths.write[i] = lengths[i - 1] + p_points[i].distance_to(p_points[i - 1]);
	}
	Vector<Vector3> result;
	result.resize(p_count);
	int segment = 1;
	for (int i = 0; i < p_count; i++) {
		real_t distance = lengths[lengths.size() - 1] * i / (p_count - 1);
		while (segment < p_points.size() - 1 && lengths[segment] < distance) {
			segment++;
		}
		real_t span = lengths[segment] - lengths[segment - 1];
		result.write[i] = span > CMP_EPSILON ? p_points[segment - 1].lerp(p_points[segment], (distance - lengths[segment - 1]) / span) : p_points[segment];
	}
	return result;
}

void Rope3DSolver::reset(const Vector<Vector3> &p_points, bool p_capture_rest) {
	particles.resize(p_points.size());
	segments.resize(p_points.size() - 1);
	rest_angles.resize(p_points.size() - 2);
	bend_lambdas.resize(p_points.size() - 2);
	for (int i = 0; i < particles.size(); i++) {
		Particle &particle = particles.write[i];
		particle.position = p_points[i];
		particle.previous = p_points[i];
		particle.velocity = Vector3();
		particle.attachment_lambda = Vector3();
		particle.pose_lambda = Vector3();
	}
	if (p_capture_rest) {
		for (int i = 0; i < segments.size(); i++) {
			Vector3 direction = p_points[i + 1] - p_points[i];
			segments.write[i].direction = direction.length_squared() > CMP_EPSILON * CMP_EPSILON ? direction.normalized() : Vector3(0, -1, 0);
		}
		for (int i = 0; i < rest_angles.size(); i++) {
			rest_angles.write[i] = Math::acos(CLAMP(segments[i].direction.dot(segments[i + 1].direction), -1.0, 1.0));
		}
	}
	update_lengths();
}

void Rope3DSolver::update_lengths() {
	if (segments.is_empty()) {
		return;
	}
	// Arc-length resampling gives uniform segments. Compliance is authored for the whole rope.
	real_t length = rest_length / segments.size();
	for (int i = 0; i < segments.size(); i++) {
		segments.write[i].length = length;
		segments.write[i].lambda = 0;
	}
	for (int i = 0; i < particles.size(); i++) {
		real_t fraction = (i == 0 || i == particles.size() - 1) ? 0.5 : 1.0;
		particles.write[i].inverse_mass = segments.size() / (mass * fraction);
	}
	for (int i = 0; i < bend_lambdas.size(); i++) {
		bend_lambdas.write[i] = 0;
	}
}

real_t Rope3DSolver::weight(int p_index) const {
	const Particle &particle = particles[p_index];
	return particle.attached && particle.compliance == 0 ? 0 : particle.inverse_mass;
}

void Rope3DSolver::predict(real_t p_delta) {
	for (int i = 0; i < particles.size(); i++) {
		Particle &particle = particles.write[i];
		particle.previous = particle.position;
		particle.attachment_lambda = Vector3();
		particle.pose_lambda = Vector3();
		if (weight(i) > 0) {
			particle.velocity = particle.velocity * Math::exp(-damping * p_delta) + gravity * p_delta;
			particle.position += particle.velocity * p_delta;
		}
	}
	for (int i = 0; i < segments.size(); i++) {
		segments.write[i].lambda = 0;
	}
	for (int i = 0; i < bend_lambdas.size(); i++) {
		bend_lambdas.write[i] = 0;
	}
	restore_pins();
}

void Rope3DSolver::restore_pins() {
	for (int i = 0; i < particles.size(); i++) {
		Particle &particle = particles.write[i];
		if (particle.attached && particle.compliance == 0) {
			particle.position = particle.target;
		}
	}
}

void Rope3DSolver::project(real_t p_delta, bool p_reverse) {
	real_t dt_squared = p_delta * p_delta;
	// Attachments/pose, alternating stretch, rest-angle bend, then caller contacts and exact pins.
	for (int j = 0; j < particles.size(); j++) {
		int i = p_reverse ? particles.size() - 1 - j : j;
		Particle &particle = particles.write[i];
		real_t w = weight(i);
		if (w == 0) {
			continue;
		}
		if (particle.attached) {
			real_t alpha = particle.compliance / dt_squared;
			Vector3 dlambda = (particle.target - particle.position - particle.attachment_lambda * alpha) / (w + alpha);
			particle.attachment_lambda += dlambda;
			particle.position += dlambda * w;
		}
		if (!particle.attached && pose.size() == particles.size()) {
			real_t alpha = pose_compliance / dt_squared;
			Vector3 dlambda = (pose[i] - particle.position - particle.pose_lambda * alpha) / (w + alpha);
			particle.pose_lambda += dlambda;
			particle.position += dlambda * w;
		}
	}
	for (int j = 0; j < segments.size(); j++) {
		int i = p_reverse ? segments.size() - 1 - j : j;
		Segment &segment = segments.write[i];
		Vector3 separation = particles[i + 1].position - particles[i].position;
		real_t distance = separation.length();
		Vector3 direction = distance > CMP_EPSILON ? separation / distance : segment.direction;
		real_t w0 = weight(i);
		real_t w1 = weight(i + 1);
		real_t alpha = stretch_compliance / segments.size() / dt_squared;
		if (w0 + w1 + alpha <= 0) {
			continue;
		}
		real_t dlambda = (segment.length - distance - alpha * segment.lambda) / (w0 + w1 + alpha);
		segment.lambda += dlambda;
		particles.write[i].position -= direction * (w0 * dlambda);
		particles.write[i + 1].position += direction * (w1 * dlambda);
	}
	if (bend_enabled) {
		for (int j = 0; j < rest_angles.size(); j++) {
			int i = p_reverse ? rest_angles.size() - 1 - j : j;
			Vector3 a = particles[i + 1].position - particles[i].position;
			Vector3 b = particles[i + 2].position - particles[i + 1].position;
			real_t la = a.length();
			real_t lb = b.length();
			if (la < CMP_EPSILON || lb < CMP_EPSILON) {
				continue;
			}
			a /= la;
			b /= lb;
			real_t cosine = CLAMP(a.dot(b), -1.0, 1.0);
			Vector3 axis = a.cross(b);
			if (axis.length_squared() < CMP_EPSILON * CMP_EPSILON) {
				axis = a.cross(Math::abs(a.y) < 0.9 ? Vector3(0, 1, 0) : Vector3(1, 0, 0)).normalized();
			} else {
				axis.normalize();
			}
			Vector3 g0 = -a.cross(axis) / la;
			Vector3 g2 = -b.cross(axis) / lb;
			Vector3 g1 = -g0 - g2;
			real_t w0 = weight(i), w1 = weight(i + 1), w2 = weight(i + 2);
			real_t alpha = bend_compliance * (rest_length / segments.size()) / dt_squared;
			real_t denominator = w0 * g0.length_squared() + w1 * g1.length_squared() + w2 * g2.length_squared() + alpha;
			if (denominator <= 0) {
				continue;
			}
			real_t dlambda = (rest_angles[i] - Math::acos(cosine) - alpha * bend_lambdas[i]) / denominator;
			bend_lambdas.write[i] += dlambda;
			particles.write[i].position += g0 * (w0 * dlambda);
			particles.write[i + 1].position += g1 * (w1 * dlambda);
			particles.write[i + 2].position += g2 * (w2 * dlambda);
		}
	}
	restore_pins();
}

void Rope3DSolver::finish(real_t p_delta) {
	for (int i = 0; i < particles.size(); i++) {
		Particle &particle = particles.write[i];
		particle.velocity = (particle.position - particle.previous) / p_delta;
	}
}

real_t Rope3DSolver::max_error() const {
	real_t error = 0;
	for (int i = 0; i < segments.size(); i++) {
		error = MAX(error, Math::abs(particles[i].position.distance_to(particles[i + 1].position) - segments[i].length));
	}
	return error;
}

bool Rope3DSolver::overstretched() const {
	int previous_pin = -1;
	for (int i = 0; i < particles.size(); i++) {
		if (weight(i) == 0) {
			if (previous_pin >= 0 && particles[previous_pin].target.distance_to(particles[i].target) > rest_length * (i - previous_pin) / segments.size() + CMP_EPSILON) {
				return true;
			}
			previous_pin = i;
		}
	}
	return false;
}
