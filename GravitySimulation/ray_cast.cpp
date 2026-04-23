#include "ray_cast.h"

#include <algorithm>

#include "Renderer.h"
#include "Scene.h"
#include "collider.h"
#include "scene_node.h"

ray_cast::ray_cast(const world_ray& ray, collision_mask_t query_mask)
    : ray_(ray), query_mask_(query_mask) {
}

void ray_cast::set_ray(const world_ray& ray) {
    ray_ = ray;
}

void ray_cast::set_query_mask(collision_mask_t query_mask) {
    query_mask_ = query_mask;
}

void ray_cast::clear_hits() {
    hits_.clear();
}

const ray_cast_hit* ray_cast::get_closest_hit() const {
    if (hits_.empty())
        return nullptr;
    return &hits_.front();
}

bool ray_cast::cast(const scene& scene_context, bool stop_on_first_hit) {
    clear_hits();

    for (auto* collider_component : scene_context.get_colliders()) {
        if (!collider_component || !collider_component->is_enabled() || !collider_component->get_node())
            continue;

        auto* node = collider_component->get_node();
        if (!collision_mask_matches(node->get_collision_layer_mask(), query_mask_))
            continue;

        ray_cast_hit hit;
        if (!collider_component->raycast(ray_, hit))
            continue;

        if (!hit.render)
            hit.render = node->find_component<renderer>();

        hits_.push_back(hit);
    }

    std::sort(hits_.begin(), hits_.end(), [](const ray_cast_hit& lhs, const ray_cast_hit& rhs) {
        return lhs.distance < rhs.distance;
    });

    if (stop_on_first_hit && hits_.size() > 1)
        hits_.resize(1);

    return !hits_.empty();
}
