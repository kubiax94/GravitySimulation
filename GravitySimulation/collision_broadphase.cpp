#include "collision_broadphase.h"

#include <algorithm>

#include "collider.h"
#include "scene_node.h"

std::vector<broadphase_proxy> collision_broadphase::build_proxies(const std::vector<collider*>& colliders) const {
    std::vector<broadphase_proxy> proxies;
    proxies.reserve(colliders.size());

    for (auto* collider_component : colliders) {
        if (!collider_component)
            continue;

        auto* node = collider_component->get_node();
        if (!node)
            continue;

        broadphase_proxy proxy;
        proxy.collider_component = collider_component;
        proxy.node = node;
        proxy.world_bounds = collider_component->get_world_bounds();
        proxy.layer_mask = node->get_collision_layer_mask();
        proxy.query_mask = node->get_collision_query_mask();
        proxy.enabled = collider_component->is_enabled();

        if (!proxy.is_valid())
            continue;

        proxies.push_back(proxy);
    }

    return proxies;
}

std::vector<broadphase_pair> collision_broadphase::find_pairs(const std::vector<broadphase_proxy>& proxies) const {
    std::vector<const broadphase_proxy*> sorted_proxies;
    sorted_proxies.reserve(proxies.size());

    for (const auto& proxy : proxies) {
        if (proxy.is_valid())
            sorted_proxies.push_back(&proxy);
    }

    std::sort(sorted_proxies.begin(), sorted_proxies.end(), [](const broadphase_proxy* lhs, const broadphase_proxy* rhs) {
        if (lhs->world_bounds.min.x == rhs->world_bounds.min.x)
            return lhs->world_bounds.max.x < rhs->world_bounds.max.x;
        return lhs->world_bounds.min.x < rhs->world_bounds.min.x;
    });

    std::vector<broadphase_pair> pairs;
    pairs.reserve(sorted_proxies.size());

    for (size_t i = 0; i < sorted_proxies.size(); ++i) {
        const auto* first = sorted_proxies[i];
        if (!first)
            continue;

        for (size_t j = i + 1; j < sorted_proxies.size(); ++j) {
            const auto* second = sorted_proxies[j];
            if (!second)
                continue;

            if (second->world_bounds.min.x > first->world_bounds.max.x)
                break;

            if (first->node == second->node)
                continue;

            if (!collision_pair_matches(first->layer_mask, first->query_mask, second->layer_mask, second->query_mask))
                continue;

            pairs.push_back({ first->collider_component, second->collider_component });
        }
    }

    return pairs;
}
