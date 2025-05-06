#pragma once
#include "Scene.h"

class renderer;

namespace simtest
{
	void init_gravity_test(scene* s_to_init, std::vector<renderer*>& planets_renders);
}

