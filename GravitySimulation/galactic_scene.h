#pragma once

#include <vector>

#include "Scene.h"

class renderer;

class galactic_scene final : public scene
{
    std::vector<renderer*> planet_renderers_;

    void initialize_scene_content();

public:
    explicit galactic_scene(sim::time* time);
    ~galactic_scene() override = default;
};
