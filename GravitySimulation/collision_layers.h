#pragma once

#include <cstdint>

using collision_mask_t = std::uint32_t;

constexpr collision_mask_t collision_mask_all = 0xffffffffu;

// Single-bit collision layers. Keep this generic so scene systems can reuse it.
enum class collision_layer : collision_mask_t
{
    default_layer = 1u << 0,
    terrain = 1u << 1,
    character = 1u << 2,
    environment = 1u << 3,
    fluid = 1u << 4,
    sensor = 1u << 5,
    debug = 1u << 6
};

constexpr collision_mask_t to_collision_mask(collision_layer layer) {
    return static_cast<collision_mask_t>(layer);
}

constexpr bool collision_mask_matches(collision_mask_t object_layer_mask, collision_mask_t query_mask) {
    return (object_layer_mask & query_mask) != 0u;
}

constexpr bool collision_pair_matches(collision_mask_t layer_a, collision_mask_t mask_a, collision_mask_t layer_b, collision_mask_t mask_b) {
    return collision_mask_matches(layer_a, mask_b) && collision_mask_matches(layer_b, mask_a);
}
