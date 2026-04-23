#pragma once

#include <algorithm>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "bounding_box.h"

struct world_ray
{
    glm::vec3 origin = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
};

struct view_frustum
{
    glm::vec4 planes[6]{};
};

inline glm::vec4 normalize_plane(const glm::vec4& plane) {
    const glm::vec3 normal(plane.x, plane.y, plane.z);
    const float length = glm::length(normal);
    if (length <= 0.000001f)
        return plane;
    return plane / length;
}

inline view_frustum build_view_frustum(const glm::mat4& view_projection) {
    view_frustum frustum;
    const glm::mat4 rows = glm::transpose(view_projection);
    frustum.planes[0] = normalize_plane(rows[3] + rows[0]);
    frustum.planes[1] = normalize_plane(rows[3] - rows[0]);
    frustum.planes[2] = normalize_plane(rows[3] + rows[1]);
    frustum.planes[3] = normalize_plane(rows[3] - rows[1]);
    frustum.planes[4] = normalize_plane(rows[3] + rows[2]);
    frustum.planes[5] = normalize_plane(rows[3] - rows[2]);
    return frustum;
}

inline bool intersects(const view_frustum& frustum, const bounding_box& box) {
    if (!box.valid)
        return true;

    const glm::vec3 center = box.get_center();
    const glm::vec3 extents = box.get_size() * 0.5f;
    for (const glm::vec4& plane : frustum.planes) {
        const glm::vec3 normal(plane.x, plane.y, plane.z);
        const float radius = glm::dot(glm::abs(normal), extents);
        const float distance = glm::dot(normal, center) + plane.w;
        if (distance + radius < 0.0f)
            return false;
    }

    return true;
}

inline world_ray make_world_ray_from_screen(const glm::vec2& mouse_position, const glm::ivec2& viewport_size, const glm::mat4& inverse_view_projection) {
    const float width = static_cast<float>(std::max(viewport_size.x, 1));
    const float height = static_cast<float>(std::max(viewport_size.y, 1));
    const float ndc_x = (2.0f * mouse_position.x) / width - 1.0f;
    const float ndc_y = 1.0f - (2.0f * mouse_position.y) / height;

    glm::vec4 near_clip(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 far_clip(ndc_x, ndc_y, 1.0f, 1.0f);
    glm::vec4 near_world = inverse_view_projection * near_clip;
    glm::vec4 far_world = inverse_view_projection * far_clip;
    near_world /= std::max(near_world.w, 0.000001f);
    far_world /= std::max(far_world.w, 0.000001f);

    world_ray ray;
    ray.origin = glm::vec3(near_world);
    ray.direction = glm::normalize(glm::vec3(far_world - near_world));
    return ray;
}

inline bool intersect_ray_aabb(const world_ray& ray, const bounding_box& box, float& hit_t_min, float& hit_t_max) {
    if (!box.valid)
        return false;

    hit_t_min = 0.0f;
    hit_t_max = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        const float origin = ray.origin[axis];
        const float direction = ray.direction[axis];
        const float box_min = box.min[axis];
        const float box_max = box.max[axis];

        if (std::abs(direction) <= 0.000001f) {
            if (origin < box_min || origin > box_max)
                return false;
            continue;
        }

        float t0 = (box_min - origin) / direction;
        float t1 = (box_max - origin) / direction;
        if (t0 > t1)
            std::swap(t0, t1);

        hit_t_min = std::max(hit_t_min, t0);
        hit_t_max = std::min(hit_t_max, t1);
        if (hit_t_min > hit_t_max)
            return false;
    }

    return hit_t_max >= 0.0f;
}
