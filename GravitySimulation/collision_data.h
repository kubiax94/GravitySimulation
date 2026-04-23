#pragma once

#include <cstdint>

#include <glm/vec4.hpp>
#include <glm/gtc/type_precision.hpp>

constexpr uint32_t collision_flag_enabled = 1u << 0;
constexpr uint32_t collision_flag_trigger = 1u << 1;
constexpr uint32_t collision_flag_dynamic = 1u << 2;
constexpr uint32_t collision_invalid_body_index = 0xffffffffu;

struct collision_data
{
    glm::vec4 center = glm::vec4(0.0f);
    glm::vec4 half_extents = glm::vec4(0.0f);
    glm::uvec4 metadata = glm::uvec4(collision_invalid_body_index, 0u, 0u, 0u);
};
