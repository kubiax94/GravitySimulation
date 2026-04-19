#pragma once
#include "Scene.h"
#include "physics_data.h"
#include "Renderer.h"

namespace simtest
{
  std::vector<physics_data> create_stress_particles(int count);
	void init_gravity_test(scene* s_to_init, std::vector<renderer*>& planets_renders);

	// create 'count' random bodies for stress testing (adds rigid bodies + renderers)
	void stress_test(scene* s_to_init, std::vector<renderer*>& planets_renders, int count);
}

