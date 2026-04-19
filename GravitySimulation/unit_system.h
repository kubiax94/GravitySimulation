#pragma once
struct unit_system
{
 // Gravitational constant expressed in km^3 / (kg * s^2), because distance_scale uses kilometers.
	static const float const_G;
	const float scaled_G() const;
	float mass_scale;
   float distance_scale;
	float scale_time;

	// Converts kilograms to simulation mass units.
   float mass(float kg) const;
	// Converts kilometers to simulation distance units.
   float distance(float km) const;
	// Converts seconds to simulation time units.
  float time(float seconds) const;

   float to_renderer_scale(float realm_km) const {
		return realm_km / 12742.0f;
	}

};

