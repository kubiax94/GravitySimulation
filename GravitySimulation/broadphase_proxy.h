#pragma once

#include "bounding_box.h"
#include "collision_layers.h"

class collider;
class scene_node;

struct broadphase_proxy
{
    collider* collider_component = nullptr;
    scene_node* node = nullptr;
    bounding_box world_bounds;
    collision_mask_t layer_mask = 0u;
    collision_mask_t query_mask = 0u;
    bool enabled = false;

    [[nodiscard]] bool is_valid() const {
        return collider_component != nullptr && node != nullptr && enabled && world_bounds.valid;
    }
};
