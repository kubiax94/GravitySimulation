#pragma once
#ifndef TIME_H_


namespace sim
{
	

class time
{
private:
	float accumulator_ = 0;
	float last_frame_ = 0;

public:
	float current;
	float delta_time;
	const float fixed_delta_time = 1.f / 60.f;
	[[nodiscard]] bool should_fixed_update() const;
	void reduce_accumulator();
	void update_time(float new_frame_time);
};
}

#endif // !TIME_H_

