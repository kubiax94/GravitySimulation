#pragma once
struct unit_system
{
	static const float const_G;
	const float scaled_G() const;
	float mass_scale;
	float distance_scale;
	float scale_time;

	float mass(float kg);
	float distance(float km);
	float time(float seconds);

	float to_renderer_scale(float realm_km) {
		return realm_km / 12742.0f;
	}

};

