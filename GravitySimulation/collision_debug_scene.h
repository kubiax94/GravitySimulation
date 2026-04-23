#pragma once

#include "Scene.h"

class collision_debug_scene final : public scene
{
    compute_shader* debug_gravity_compute_shader_ = nullptr;
    void initialize_scene_content();

public:
    explicit collision_debug_scene(sim::time* time);
    void update() override;
    ~collision_debug_scene() override = default;
};
