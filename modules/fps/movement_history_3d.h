/**************************************************************************/
/*  movement_history_3d.h                                                 */
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

#include "scene/main/node.h"

class MovementHistory3D : public Node {
	GDCLASS(MovementHistory3D, Node);

public:
	enum {
		HISTORY_SIZE = 24,
		COMMAND_LIMIT = 8,
		COMMAND_BYTES = 29,
		SAMPLE_BYTES = 54,
	};

private:
	struct Sample {
		double time = 0;
		Vector3 position;
		Vector3 velocity;
		double yaw = 0;
		int64_t ack = 0;
		int64_t epoch = 0;
		double gait_phase = 0;
		bool gait_advancing = false;
		bool grounded = false;
		bool crouching = false;
		bool jumping = false;
	};
	Sample samples[HISTORY_SIZE];
	int first = 0;
	int count = 0;
	double cursor = 0;
	double render_delay = 0.1;
	bool initialized = false;

	const Sample &_sample(int p_index) const;
	bool _push(const Sample *p_samples, int p_count);
	static bool _valid(const Sample &p_sample);
	static bool _from_dictionary(const Dictionary &p_value, Sample &r_sample);
	static Dictionary _to_dictionary(const Sample &p_sample);
	static bool _decode_samples(const PackedByteArray &p_packet, Sample *r_samples, int &r_count);
	static void _encode_sample(const Sample &p_sample, uint8_t *r_bytes);

protected:
	static void _bind_methods();

public:
	void clear();
	int get_sample_count() const { return count; }
	void set_render_delay(double p_delay);
	double get_render_delay() const { return render_delay; }
	bool push_sample(const Dictionary &p_sample);
	bool push_samples(const Array &p_samples);
	bool push_packet(const PackedByteArray &p_packet);
	PackedByteArray to_packet() const;
	Dictionary get_latest_sample() const;
	Dictionary sample_at(double p_time) const;
	Dictionary advance(double p_delta);
	static PackedByteArray encode_samples(const Array &p_samples);
	static Array decode_samples(const PackedByteArray &p_packet);
	static PackedByteArray encode_commands(const Array &p_commands);
	static Array decode_commands(const PackedByteArray &p_packet);
};
