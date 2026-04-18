#pragma once
#include "Scene.h"

class renderer;

namespace simtest
{
	void init_gravity_test(scene* s_to_init, std::vector<renderer*>& planets_renders);

	// create 'count' random bodies for stress testing (adds rigid bodies + renderers)
	void stress_test(scene* s_to_init, std::vector<renderer*>& planets_renders, int count);
}

