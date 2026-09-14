/**************************************************************************/
/*  movement_history_3d.cpp                                               */
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

#include "movement_history_3d.h"

#include "core/io/marshalls.h"
#include "core/object/class_db.h"

namespace {

bool has_type(const Dictionary &p_value, const StringName &p_key, Variant::Type p_type) {
	const Variant *value = p_value.getptr(p_key);
	return value && value->get_type() == p_type;
}

bool valid_command(const Dictionary &p_command, int64_t p_previous, int64_t p_previous_jump) {
	for (const char *key : { "sequence", "jump_id" }) {
		if (!has_type(p_command, key, Variant::INT)) {
			return false;
		}
	}
	if (!has_type(p_command, "movement", Variant::VECTOR2) || !has_type(p_command, "yaw", Variant::FLOAT)) {
		return false;
	}
	for (const char *key : { "jump", "crouch", "sprint", "slow" }) {
		if (!has_type(p_command, key, Variant::BOOL)) {
			return false;
		}
	}
	int64_t sequence = p_command["sequence"];
	int64_t jump_id = p_command["jump_id"];
	Vector2 movement = p_command["movement"];
	double yaw = p_command["yaw"];
	return sequence > p_previous && jump_id >= p_previous_jump && jump_id <= sequence &&
			movement.is_finite() && movement.length() <= 1.001 && Math::is_finite(yaw) && Math::abs(yaw) <= Math::PI + 0.000001;
}

} // namespace

void MovementHistory3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &MovementHistory3D::clear);
	ClassDB::bind_method(D_METHOD("get_sample_count"), &MovementHistory3D::get_sample_count);
	ClassDB::bind_method(D_METHOD("set_render_delay", "delay"), &MovementHistory3D::set_render_delay);
	ClassDB::bind_method(D_METHOD("get_render_delay"), &MovementHistory3D::get_render_delay);
	ClassDB::bind_method(D_METHOD("push_sample", "sample"), &MovementHistory3D::push_sample);
	ClassDB::bind_method(D_METHOD("push_samples", "samples"), &MovementHistory3D::push_samples);
	ClassDB::bind_method(D_METHOD("push_packet", "packet"), &MovementHistory3D::push_packet);
	ClassDB::bind_method(D_METHOD("to_packet"), &MovementHistory3D::to_packet);
	ClassDB::bind_method(D_METHOD("get_latest_sample"), &MovementHistory3D::get_latest_sample);
	ClassDB::bind_method(D_METHOD("sample_at", "time"), &MovementHistory3D::sample_at);
	ClassDB::bind_method(D_METHOD("advance", "delta"), &MovementHistory3D::advance);
	ClassDB::bind_static_method("MovementHistory3D", D_METHOD("encode_samples", "samples"), &MovementHistory3D::encode_samples);
	ClassDB::bind_static_method("MovementHistory3D", D_METHOD("decode_samples", "packet"), &MovementHistory3D::decode_samples);
	ClassDB::bind_static_method("MovementHistory3D", D_METHOD("encode_commands", "commands"), &MovementHistory3D::encode_commands);
	ClassDB::bind_static_method("MovementHistory3D", D_METHOD("decode_commands", "packet"), &MovementHistory3D::decode_commands);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "render_delay", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater,suffix:s"), "set_render_delay", "get_render_delay");
	BIND_CONSTANT(HISTORY_SIZE);
	BIND_CONSTANT(COMMAND_LIMIT);
	BIND_CONSTANT(COMMAND_BYTES);
	BIND_CONSTANT(SAMPLE_BYTES);
}

bool MovementHistory3D::_valid(const Sample &p_sample) {
	return Math::is_finite(p_sample.time) && p_sample.time >= 0 && p_sample.position.is_finite() && p_sample.position.length() <= 1000000.0 &&
			p_sample.velocity.is_finite() && Math::is_finite(float(p_sample.velocity.x)) && Math::is_finite(float(p_sample.velocity.y)) && Math::is_finite(float(p_sample.velocity.z)) &&
			Math::is_finite(p_sample.yaw) && Math::is_finite(float(p_sample.yaw)) && p_sample.ack >= 0 && p_sample.epoch >= 0 && p_sample.epoch <= INT32_MAX &&
			Math::is_finite(p_sample.gait_phase) && p_sample.gait_phase >= 0 && p_sample.gait_phase < 1;
}

bool MovementHistory3D::_from_dictionary(const Dictionary &p_value, Sample &r_sample) {
	for (const char *key : { "time", "yaw", "gait_phase" }) {
		if (!has_type(p_value, key, Variant::FLOAT)) {
			return false;
		}
	}
	for (const char *key : { "position", "velocity" }) {
		if (!has_type(p_value, key, Variant::VECTOR3)) {
			return false;
		}
	}
	for (const char *key : { "ack", "epoch" }) {
		if (!has_type(p_value, key, Variant::INT)) {
			return false;
		}
	}
	for (const char *key : { "grounded", "crouching", "gait_advancing" }) {
		if (!has_type(p_value, key, Variant::BOOL)) {
			return false;
		}
	}
	if (p_value.has("jumping") && !has_type(p_value, "jumping", Variant::BOOL)) {
		return false;
	}
	r_sample.time = p_value["time"];
	r_sample.position = p_value["position"];
	r_sample.velocity = p_value["velocity"];
	r_sample.yaw = p_value["yaw"];
	r_sample.ack = p_value["ack"];
	r_sample.epoch = p_value["epoch"];
	r_sample.gait_phase = p_value["gait_phase"];
	r_sample.gait_advancing = p_value["gait_advancing"];
	r_sample.grounded = p_value["grounded"];
	r_sample.crouching = p_value["crouching"];
	r_sample.jumping = p_value.get("jumping", false);
	return _valid(r_sample);
}

Dictionary MovementHistory3D::_to_dictionary(const Sample &p_sample) {
	Dictionary result;
	result["time"] = p_sample.time;
	result["position"] = p_sample.position;
	result["velocity"] = p_sample.velocity;
	result["yaw"] = p_sample.yaw;
	result["ack"] = p_sample.ack;
	result["epoch"] = p_sample.epoch;
	result["gait_phase"] = p_sample.gait_phase;
	result["gait_advancing"] = p_sample.gait_advancing;
	result["grounded"] = p_sample.grounded;
	result["crouching"] = p_sample.crouching;
	result["jumping"] = p_sample.jumping;
	return result;
}

const MovementHistory3D::Sample &MovementHistory3D::_sample(int p_index) const {
	return samples[(first + p_index) % HISTORY_SIZE];
}

void MovementHistory3D::clear() {
	first = 0;
	count = 0;
	cursor = 0;
	initialized = false;
}

void MovementHistory3D::set_render_delay(double p_delay) {
	ERR_FAIL_COND(!Math::is_finite(p_delay) || p_delay < 0);
	render_delay = p_delay;
}

bool MovementHistory3D::_push(const Sample *p_samples, int p_count) {
	if (count && p_samples[p_count - 1].time <= _sample(count - 1).time) {
		return false;
	}
	int64_t epoch = p_samples[p_count - 1].epoch;
	if (count && _sample(count - 1).epoch != epoch) {
		clear();
	}
	for (int i = 0; i < p_count; i++) {
		const Sample &sample = p_samples[i];
		if (sample.epoch != epoch || (count && sample.time <= _sample(count - 1).time)) {
			continue;
		}
		if (count == HISTORY_SIZE) {
			first = (first + 1) % HISTORY_SIZE;
			count--;
		}
		samples[(first + count) % HISTORY_SIZE] = sample;
		count++;
	}
	if (!initialized) {
		cursor = MAX(_sample(0).time, _sample(count - 1).time - render_delay);
		initialized = true;
	}
	return true;
}

bool MovementHistory3D::push_sample(const Dictionary &p_sample) {
	Sample sample;
	return _from_dictionary(p_sample, sample) && _push(&sample, 1);
}

bool MovementHistory3D::push_samples(const Array &p_samples) {
	if (p_samples.is_empty() || p_samples.size() > HISTORY_SIZE) {
		return false;
	}
	Sample batch[HISTORY_SIZE];
	for (int i = 0; i < p_samples.size(); i++) {
		if (p_samples[i].get_type() != Variant::DICTIONARY || !_from_dictionary(p_samples[i], batch[i]) || (i && batch[i].time <= batch[i - 1].time)) {
			return false;
		}
	}
	return _push(batch, p_samples.size());
}

Dictionary MovementHistory3D::get_latest_sample() const {
	return count ? _to_dictionary(_sample(count - 1)) : Dictionary();
}

Dictionary MovementHistory3D::sample_at(double p_time) const {
	if (!count || !Math::is_finite(p_time)) {
		return Dictionary();
	}
	if (p_time <= _sample(0).time) {
		return _to_dictionary(_sample(0));
	}
	for (int i = 1; i < count; i++) {
		const Sample &right = _sample(i);
		if (p_time > right.time) {
			continue;
		}
		const Sample &left = _sample(i - 1);
		double weight = (p_time - left.time) / (right.time - left.time);
		Sample result = left;
		result.position = left.position.lerp(right.position, weight);
		result.velocity = left.velocity.lerp(right.velocity, weight);
		result.yaw = Math::lerp_angle(left.yaw, right.yaw, weight);
		result.gait_phase = Math::fposmod(left.gait_phase + Math::wrapf(right.gait_phase - left.gait_phase, -0.5, 0.5) * weight, 1.0);
		result.gait_advancing = weight < 1.0 ? left.gait_advancing : right.gait_advancing;
		return _to_dictionary(result);
	}
	return get_latest_sample();
}

Dictionary MovementHistory3D::advance(double p_delta) {
	if (!count || !Math::is_finite(p_delta) || p_delta < 0) {
		return Dictionary();
	}
	double latest = _sample(count - 1).time;
	double rate = CLAMP(1.0 + (latest - render_delay - cursor) * 4.0, 0.9, 1.1);
	cursor = CLAMP(cursor + p_delta * rate, _sample(0).time, latest);
	return sample_at(cursor);
}

void MovementHistory3D::_encode_sample(const Sample &p_sample, uint8_t *r_bytes) {
	encode_double(p_sample.time, r_bytes);
	encode_float(p_sample.position.x, r_bytes + 8);
	encode_float(p_sample.position.y, r_bytes + 12);
	encode_float(p_sample.position.z, r_bytes + 16);
	encode_float(p_sample.velocity.x, r_bytes + 20);
	encode_float(p_sample.velocity.y, r_bytes + 24);
	encode_float(p_sample.velocity.z, r_bytes + 28);
	encode_float(p_sample.yaw, r_bytes + 32);
	encode_uint64(p_sample.ack, r_bytes + 36);
	encode_uint32(p_sample.epoch, r_bytes + 44);
	encode_float(MIN(float(p_sample.gait_phase), 0x1.fffffep-1f), r_bytes + 48);
	r_bytes[52] = p_sample.gait_advancing;
	r_bytes[53] = int(p_sample.grounded) | (int(p_sample.crouching) << 1) | (int(p_sample.jumping) << 2);
}

bool MovementHistory3D::_decode_samples(const PackedByteArray &p_packet, Sample *r_samples, int &r_count) {
	if (p_packet.is_empty() || p_packet.size() % SAMPLE_BYTES || p_packet.size() > SAMPLE_BYTES * HISTORY_SIZE) {
		return false;
	}
	r_count = p_packet.size() / SAMPLE_BYTES;
	const uint8_t *bytes = p_packet.ptr();
	for (int i = 0; i < r_count; i++, bytes += SAMPLE_BYTES) {
		Sample &sample = r_samples[i];
		sample.time = decode_double(bytes);
		sample.position = Vector3(decode_float(bytes + 8), decode_float(bytes + 12), decode_float(bytes + 16));
		sample.velocity = Vector3(decode_float(bytes + 20), decode_float(bytes + 24), decode_float(bytes + 28));
		sample.yaw = decode_float(bytes + 32);
		uint64_t ack = decode_uint64(bytes + 36);
		if (ack > INT64_MAX || bytes[52] > 1 || bytes[53] > 7) {
			return false;
		}
		sample.ack = ack;
		sample.epoch = decode_uint32(bytes + 44);
		sample.gait_phase = decode_float(bytes + 48);
		sample.gait_advancing = bytes[52];
		sample.grounded = bytes[53] & 1;
		sample.crouching = bytes[53] & 2;
		sample.jumping = bytes[53] & 4;
		if (!_valid(sample) || (i && sample.time <= r_samples[i - 1].time)) {
			return false;
		}
	}
	return true;
}

bool MovementHistory3D::push_packet(const PackedByteArray &p_packet) {
	Sample batch[HISTORY_SIZE];
	int batch_count = 0;
	return _decode_samples(p_packet, batch, batch_count) && _push(batch, batch_count);
}

PackedByteArray MovementHistory3D::to_packet() const {
	PackedByteArray packet;
	packet.resize(count * SAMPLE_BYTES);
	uint8_t *bytes = packet.ptrw();
	for (int i = 0; i < count; i++) {
		_encode_sample(_sample(i), bytes + i * SAMPLE_BYTES);
	}
	return packet;
}

PackedByteArray MovementHistory3D::encode_samples(const Array &p_samples) {
	if (p_samples.is_empty() || p_samples.size() > HISTORY_SIZE) {
		return PackedByteArray();
	}
	Sample batch[HISTORY_SIZE];
	for (int i = 0; i < p_samples.size(); i++) {
		if (p_samples[i].get_type() != Variant::DICTIONARY || !_from_dictionary(p_samples[i], batch[i]) || (i && batch[i].time <= batch[i - 1].time)) {
			return PackedByteArray();
		}
	}
	PackedByteArray packet;
	packet.resize(p_samples.size() * SAMPLE_BYTES);
	uint8_t *bytes = packet.ptrw();
	for (int i = 0; i < p_samples.size(); i++) {
		_encode_sample(batch[i], bytes + i * SAMPLE_BYTES);
	}
	return packet;
}

Array MovementHistory3D::decode_samples(const PackedByteArray &p_packet) {
	Sample batch[HISTORY_SIZE];
	int batch_count = 0;
	if (!_decode_samples(p_packet, batch, batch_count)) {
		return Array();
	}
	Array result;
	result.resize(batch_count);
	for (int i = 0; i < batch_count; i++) {
		result[i] = _to_dictionary(batch[i]);
	}
	return result;
}

PackedByteArray MovementHistory3D::encode_commands(const Array &p_commands) {
	if (p_commands.is_empty() || p_commands.size() > COMMAND_LIMIT) {
		return PackedByteArray();
	}
	PackedByteArray packet;
	packet.resize(p_commands.size() * COMMAND_BYTES);
	uint8_t *bytes = packet.ptrw();
	int64_t previous = 0;
	int64_t previous_jump = 0;
	for (int i = 0; i < p_commands.size(); i++, bytes += COMMAND_BYTES) {
		if (p_commands[i].get_type() != Variant::DICTIONARY) {
			return PackedByteArray();
		}
		Dictionary command = p_commands[i];
		if (!valid_command(command, previous, previous_jump)) {
			return PackedByteArray();
		}
		previous = command["sequence"];
		previous_jump = command["jump_id"];
		Vector2 movement = command["movement"];
		encode_uint64(previous, bytes);
		encode_uint64(previous_jump, bytes + 8);
		encode_float(movement.x, bytes + 16);
		encode_float(movement.y, bytes + 20);
		encode_float(command["yaw"], bytes + 24);
		bytes[28] = int(bool(command["jump"])) | (int(bool(command["crouch"])) << 1) | (int(bool(command["sprint"])) << 2) | (int(bool(command["slow"])) << 3);
	}
	return packet;
}

Array MovementHistory3D::decode_commands(const PackedByteArray &p_packet) {
	if (p_packet.is_empty() || p_packet.size() % COMMAND_BYTES || p_packet.size() > COMMAND_BYTES * COMMAND_LIMIT) {
		return Array();
	}
	Array result;
	const uint8_t *bytes = p_packet.ptr();
	int64_t previous = 0;
	int64_t previous_jump = 0;
	for (int i = 0; i < p_packet.size() / COMMAND_BYTES; i++, bytes += COMMAND_BYTES) {
		uint64_t sequence = decode_uint64(bytes);
		uint64_t jump_id = decode_uint64(bytes + 8);
		if (sequence > INT64_MAX || jump_id > INT64_MAX || bytes[28] > 15) {
			return Array();
		}
		Dictionary command;
		command["sequence"] = int64_t(sequence);
		command["jump_id"] = int64_t(jump_id);
		command["movement"] = Vector2(decode_float(bytes + 16), decode_float(bytes + 20));
		command["yaw"] = double(decode_float(bytes + 24));
		command["jump"] = bool(bytes[28] & 1);
		command["crouch"] = bool(bytes[28] & 2);
		command["sprint"] = bool(bytes[28] & 4);
		command["slow"] = bool(bytes[28] & 8);
		if (!valid_command(command, previous, previous_jump)) {
			return Array();
		}
		previous = sequence;
		previous_jump = jump_id;
		result.push_back(command);
	}
	return result;
}
