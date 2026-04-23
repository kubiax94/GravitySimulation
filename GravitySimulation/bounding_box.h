#pragma once

#include <array>

#include <glm/glm.hpp>

struct bounding_box
{
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
    bool valid = false;

    void reset() {
        min = glm::vec3(0.0f);
        max = glm::vec3(0.0f);
        valid = false;
    }

    void encapsulate(const glm::vec3& point) {
        if (!valid) {
            min = point;
            max = point;
            valid = true;
            return;
        }

        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void encapsulate(const bounding_box& other) {
        if (!other.valid)
            return;

        encapsulate(other.min);
        encapsulate(other.max);
    }

    glm::vec3 get_center() const {
        return (min + max) * 0.5f;
    }

    glm::vec3 get_size() const {
        return valid ? (max - min) : glm::vec3(0.0f);
    }

    std::array<glm::vec3, 8> get_corners() const {
        return {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, max.y, max.z)
        };
    }
};

inline bounding_box transform_bounding_box(const bounding_box& box, const glm::mat4& transform) {
    bounding_box transformed;
    if (!box.valid)
        return transformed;

    for (const glm::vec3& corner : box.get_corners())
        transformed.encapsulate(glm::vec3(transform * glm::vec4(corner, 1.0f)));

    return transformed;
}
