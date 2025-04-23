#include "Time.h"

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

void sim::time::reduce_accumulator() {
	accumulator_ -= fixed_delta_time;
}