#include "collision_narrowphase.h"

#include "collider.h"

bool collision_narrowphase::build_manifold(const broadphase_pair& pair, contact_manifold& manifold) const {
    manifold = {};
    manifold.first = pair.first;
    manifold.second = pair.second;

    if (!pair.is_valid() || !pair.first || !pair.second)
        return false;

    if (pair.first->is_trigger() || pair.second->is_trigger()) {
        bounding_box overlap_bounds;
        if (!intersects(*pair.first, *pair.second, &overlap_bounds))
            return false;

        manifold.overlap_bounds = overlap_bounds;
        manifold.is_trigger = true;
        return true;
    }

    bounding_box overlap_bounds;
    if (!intersects(*pair.first, *pair.second, &overlap_bounds))
        return false;

    const glm::vec3 overlap_size = overlap_bounds.get_size();
    if (overlap_size.x <= 0.0f || overlap_size.y <= 0.0f || overlap_size.z <= 0.0f)
        return false;

    const glm::vec3 first_center = pair.first->get_world_bounds().get_center();
    const glm::vec3 second_center = pair.second->get_world_bounds().get_center();
    const glm::vec3 delta = second_center - first_center;

    manifold.overlap_bounds = overlap_bounds;
    manifold.normal = glm::vec3(delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
    float penetration = overlap_size.x;

    if (overlap_size.y < penetration) {
        penetration = overlap_size.y;
        manifold.normal = glm::vec3(0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
    }

    if (overlap_size.z < penetration) {
        penetration = overlap_size.z;
        manifold.normal = glm::vec3(0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f);
    }

    manifold.point_count = 1u;
    manifold.points[0].position = overlap_bounds.get_center();
    manifold.points[0].penetration = penetration;
    return manifold.is_valid();
}
