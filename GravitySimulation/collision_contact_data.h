#pragma once

#include <cstdint>

#include <glm/vec4.hpp>
#include <glm/gtc/type_precision.hpp>

constexpr uint32_t collision_contact_flag_active = 1u << 0;
constexpr uint32_t collision_contact_flag_trigger = 1u << 1;
constexpr uint32_t collision_contact_capacity = 4u;

struct collision_contact_data
{
   glm::uvec4 metadata[collision_contact_capacity] = {
        glm::uvec4(collision_invalid_body_index, collision_invalid_body_index, 0u, 0u),
        glm::uvec4(collision_invalid_body_index, collision_invalid_body_index, 0u, 0u),
        glm::uvec4(collision_invalid_body_index, collision_invalid_body_index, 0u, 0u),
        glm::uvec4(collision_invalid_body_index, collision_invalid_body_index, 0u, 0u)
    };
    glm::vec4 normal_penetration[collision_contact_capacity] = {
        glm::vec4(0.0f),
        glm::vec4(0.0f),
        glm::vec4(0.0f),
        glm::vec4(0.0f)
    };
};
