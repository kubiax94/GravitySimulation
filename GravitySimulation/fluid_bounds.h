#pragma once

#include <glm/vec3.hpp>

struct fluid_bounds
{
    glm::vec3 min = glm::vec3(-1.f);
    glm::vec3 max = glm::vec3(1.f);
    float restitution = 0.35f;
    float damping = 0.985f;
};
