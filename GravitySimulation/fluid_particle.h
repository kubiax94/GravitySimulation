#pragma once

#include <glm/vec4.hpp>

struct fluid_particle
{
    glm::vec4 position = glm::vec4(0.f);
    glm::vec4 velocity = glm::vec4(0.f);
    glm::vec4 predicted_position = glm::vec4(0.f);
    glm::vec4 delta_position = glm::vec4(0.f);
    glm::vec4 solver_data = glm::vec4(0.f);
};
