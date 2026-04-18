#include "Time.h"
#include <algorithm>

void sim::time::update_time(const float new_frame_time) {
	current = new_frame_time;
	delta_time = current - last_frame_;
	last_frame_ = current;

	accumulator_ += delta_time;
}

bool sim::time::should_fixed_update() const
{
	return accumulator_ >= fixed_delta_time;
}

float sim::time::interpolation_alpha() const {
	if (fixed_delta_time <= 0.f)
		return 0.f;

	return std::clamp(accumulator_ / fixed_delta_time, 0.f, 1.f);
}

void sim::time::reduce_accumulator() {
	accumulator_ -= fixed_delta_time;
}