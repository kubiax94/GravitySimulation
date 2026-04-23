#pragma once

#include <utility>

#include "bounding_box.h"

class collider;

struct collision_event
{
    collider* self = nullptr;
    collider* other = nullptr;
    bounding_box overlap_bounds;
    bool is_trigger_interaction = false;

    [[nodiscard]] bool is_valid() const {
        return self != nullptr && other != nullptr;
    }
};

inline collision_event invert_collision_event(const collision_event& event) {
    collision_event inverted = event;
    std::swap(inverted.self, inverted.other);
    return inverted;
}
