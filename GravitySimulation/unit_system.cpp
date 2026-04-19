#include "unit_system.h"

const float unit_system::const_G = 6.67430e-20f;

const float unit_system::scaled_G() const {
  return const_G * (mass_scale * scale_time * scale_time)
		/ (distance_scale * distance_scale * distance_scale);
}

float unit_system::mass(float kg) const {
	return kg / mass_scale;
}

float unit_system::distance(float km) const {
	return  km / distance_scale;
}

float unit_system::time(float seconds) const {
	return seconds / scale_time;
}
