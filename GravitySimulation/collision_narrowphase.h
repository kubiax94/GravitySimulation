#pragma once

#include "broadphase_pair.h"
#include "contact_manifold.h"

class collision_narrowphase
{
public:
    [[nodiscard]] bool build_manifold(const broadphase_pair& pair, contact_manifold& manifold) const;
};
