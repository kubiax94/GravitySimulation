#pragma once

#include <vector>

#include "Scene.h"
#include "physics_data.h"
#include "Renderer.h"

namespace galactic_stress_test
{
    std::vector<physics_data> create_stress_particles(int count);
    void initialize_stress_scene(scene* scene_to_initialize, std::vector<renderer*>& planet_renderers, int count);
}
