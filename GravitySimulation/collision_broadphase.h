#pragma once

#include <vector>

#include "broadphase_pair.h"
#include "broadphase_proxy.h"

class collider;

class collision_broadphase
{
public:
    [[nodiscard]] std::vector<broadphase_proxy> build_proxies(const std::vector<collider*>& colliders) const;
    [[nodiscard]] std::vector<broadphase_pair> find_pairs(const std::vector<broadphase_proxy>& proxies) const;
};
