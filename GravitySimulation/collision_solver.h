#pragma once

#include <unordered_map>
#include <vector>

#include <glm/vec3.hpp>

#include "collision_narrowphase.h"
#include "contact_manifold.h"
#include "rigid_body.h"
#include "uuid.h"

class collision_solver
{
public:
    [[nodiscard]] bool solve(std::vector<contact_manifold>& manifolds,
        const collision_narrowphase& narrowphase,
        const std::unordered_map<uuid, rigid_body*>& rigid_bodies_by_node_id,
        std::unordered_map<uuid, glm::vec3>& current_positions,
        int iterations) const;
};
