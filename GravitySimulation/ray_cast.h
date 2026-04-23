#pragma once

#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "collision_layers.h"
#include "spatial_query.h"

class scene;
class scene_node;
class renderer;

struct ray_cast_hit
{
    scene_node* node = nullptr;
    renderer* render = nullptr;
    bounding_box bounds;
    glm::vec3 point = glm::vec3(0.0f);
    float distance = std::numeric_limits<float>::max();
};

class ray_cast
{
    world_ray ray_{};
    collision_mask_t query_mask_ = collision_mask_all;
    std::vector<ray_cast_hit> hits_;

public:
    ray_cast() = default;
    explicit ray_cast(const world_ray& ray, collision_mask_t query_mask = collision_mask_all);

    void set_ray(const world_ray& ray);
    const world_ray& get_ray() const { return ray_; }
    void set_query_mask(collision_mask_t query_mask);
    collision_mask_t get_query_mask() const { return query_mask_; }

    void clear_hits();
    const std::vector<ray_cast_hit>& get_hits() const { return hits_; }
    const ray_cast_hit* get_closest_hit() const;
    bool cast(const scene& scene_context, bool stop_on_first_hit = false);
};
