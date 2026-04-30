#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>

#include <glm/vec3.hpp>

#include "bounding_box.h"

class collider;

struct contact_manifold_key
{
    collider* first = nullptr;
    collider* second = nullptr;

    [[nodiscard]] bool operator==(const contact_manifold_key& other) const {
        return first == other.first && second == other.second;
    }
};

struct contact_manifold_key_hash
{
    [[nodiscard]] size_t operator()(const contact_manifold_key& key) const {
        const auto first_hash = static_cast<size_t>(reinterpret_cast<std::uintptr_t>(key.first));
        const auto second_hash = static_cast<size_t>(reinterpret_cast<std::uintptr_t>(key.second));
        return first_hash ^ (second_hash << 1);
    }
};

inline contact_manifold_key make_contact_manifold_key(collider* first, collider* second) {
    if (std::less<collider*>{}(second, first))
        std::swap(first, second);

    return { first, second };
}

struct contact_point
{
    glm::vec3 position = glm::vec3(0.0f);
    float penetration = 0.0f;
    float normal_impulse_accumulated = 0.0f;
    float tangent_impulse_accumulated = 0.0f;

    [[nodiscard]] bool is_valid() const {
        return penetration > 0.0f;
    }
};

struct contact_manifold
{
    collider* first = nullptr;
    collider* second = nullptr;
    bounding_box overlap_bounds;
    glm::vec3 normal = glm::vec3(0.0f);
    std::array<contact_point, 4> points{};
    std::uint32_t point_count = 0u;
    bool is_trigger = false;
    std::uint32_t persistence = 0u;

    [[nodiscard]] bool is_valid() const {
        return first != nullptr && second != nullptr && overlap_bounds.valid && point_count > 0u && points[0].is_valid();
    }

    [[nodiscard]] float get_max_penetration() const {
        float penetration = 0.0f;
        for (std::uint32_t i = 0; i < point_count && i < points.size(); ++i)
            penetration = std::max(penetration, points[i].penetration);
        return penetration;
    }
};
