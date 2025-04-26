#include "unit_system.h"

float unit_system::mass(float kg) {
	return kg / mass_scale;
}

float unit_system::distance(float km) {
	return  km / distance_scale;
}

float unit_system::time(float seconds) {
	return seconds / scale_time;
}
