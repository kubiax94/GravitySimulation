#pragma once
struct unit_system
{
	float mass_scale;
	float distance_scale;
	float scale_time;

	float mass(float kg);
	float distance(float km);
	float time(float seconds);

};

