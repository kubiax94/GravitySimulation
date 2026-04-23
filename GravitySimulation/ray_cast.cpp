#include "ray_cast.h"

#include <algorithm>

#include "Renderer.h"
#include "Scene.h"
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

    for (auto* render : scene_context.get_renderers()) {
        if (!render || !render->get_node() || !render->get_mesh() || render->get_mesh()->type == MeshType::LINES)
            continue;

        auto* node = render->get_node();
        if (!collision_mask_matches(node->get_collision_layer_mask(), query_mask_))
            continue;

        const bounding_box world_bounds = node->get_subtree_world_bounding_box();
        float hit_t_min = 0.0f;
        float hit_t_max = 0.0f;
        if (!intersect_ray_aabb(ray_, world_bounds, hit_t_min, hit_t_max))
            continue;

        ray_cast_hit hit;
        hit.node = node;
        hit.render = render;
        hit.bounds = world_bounds;
        hit.distance = std::max(hit_t_min, 0.0f);
        hit.point = ray_.origin + ray_.direction * hit.distance;
        hits_.push_back(hit);

        if (stop_on_first_hit)
            break;
    }

    std::sort(hits_.begin(), hits_.end(), [](const ray_cast_hit& lhs, const ray_cast_hit& rhs) {
        return lhs.distance < rhs.distance;
    });
    return !hits_.empty();
}
