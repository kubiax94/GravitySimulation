#include "collision_solver.h"

#include <algorithm>
#include <cmath>

#include "broadphase_pair.h"
#include "collider.h"

namespace {
struct solver_contact_view
{
    collider* first = nullptr;
    collider* second = nullptr;
    glm::vec3 normal = glm::vec3(0.0f);
    float penetration_depth = 0.0f;

    [[nodiscard]] bool is_valid() const {
        return first != nullptr && second != nullptr && penetration_depth > 0.0f;
    }
};

solver_contact_view make_solver_contact(const contact_manifold& manifold) {
    solver_contact_view contact;
    contact.first = manifold.first;
    contact.second = manifold.second;

    if (!manifold.is_valid() || manifold.is_trigger)
        return contact;

    contact.normal = manifold.normal;
    contact.penetration_depth = manifold.get_max_penetration();
    return contact;
}
}

bool collision_solver::solve(std::vector<contact_manifold>& manifolds,
    const collision_narrowphase& narrowphase,
    const std::unordered_map<uuid, rigid_body*>& rigid_bodies_by_node_id,
    std::unordered_map<uuid, glm::vec3>& current_positions,
    int iterations) const {
    bool resolved_any = false;
    constexpr float penetration_slop = 0.0005f;
    constexpr float correction_percent = 0.7f;
    constexpr float minimum_mass = 0.0001f;
    constexpr float restitution = 0.0f;
    constexpr float static_friction = 0.5f;
    constexpr float dynamic_friction = 0.35f;

    auto apply_impulse = [](rigid_body* first_body, rigid_body* second_body,
        bool first_dynamic, bool second_dynamic,
        float first_inverse_mass, float second_inverse_mass,
        const glm::vec3& impulse) {
            if (first_dynamic && first_body)
                first_body->set_velocity(first_body->get_velocity() - impulse * first_inverse_mass);

            if (second_dynamic && second_body)
                second_body->set_velocity(second_body->get_velocity() + impulse * second_inverse_mass);
        };

    for (int iteration = 0; iteration < iterations; ++iteration) {
        bool resolved_this_iteration = false;

        for (auto& initial_manifold : manifolds) {
            if (!initial_manifold.first || !initial_manifold.second)
                continue;

            contact_manifold manifold;
            if (!narrowphase.build_manifold({ initial_manifold.first, initial_manifold.second }, manifold))
                continue;

            const solver_contact_view contact = make_solver_contact(manifold);
            if (!contact.is_valid())
                continue;

            auto* first_node = contact.first->get_node();
            auto* second_node = contact.second->get_node();
            if (!first_node || !second_node)
                continue;

            auto first_body_it = rigid_bodies_by_node_id.find(first_node->get_id());
            auto second_body_it = rigid_bodies_by_node_id.find(second_node->get_id());
            rigid_body* first_body = first_body_it != rigid_bodies_by_node_id.end() ? first_body_it->second : nullptr;
            rigid_body* second_body = second_body_it != rigid_bodies_by_node_id.end() ? second_body_it->second : nullptr;
            if (!first_body && !second_body)
                continue;

            const bool first_dynamic = first_body && first_body->get_mass() > 0.0f;
            const bool second_dynamic = second_body && second_body->get_mass() > 0.0f;
            if (!first_dynamic && !second_dynamic)
                continue;

            const float first_inverse_mass = first_dynamic ? 1.0f / std::max(first_body->get_mass(), minimum_mass) : 0.0f;
            const float second_inverse_mass = second_dynamic ? 1.0f / std::max(second_body->get_mass(), minimum_mass) : 0.0f;
            const float inverse_mass_sum = first_inverse_mass + second_inverse_mass;
            if (inverse_mass_sum <= 0.0f)
                continue;

            if (iteration == 0 && initial_manifold.point_count > 0u) {
                const glm::vec3 warm_normal_impulse = contact.normal * initial_manifold.points[0].normal_impulse_accumulated;
                apply_impulse(first_body, second_body, first_dynamic, second_dynamic, first_inverse_mass, second_inverse_mass, warm_normal_impulse);

                const glm::vec3 warm_relative_velocity = (second_body ? second_body->get_velocity() : glm::vec3(0.0f))
                    - (first_body ? first_body->get_velocity() : glm::vec3(0.0f));
                const glm::vec3 warm_tangent_velocity = warm_relative_velocity - glm::dot(warm_relative_velocity, contact.normal) * contact.normal;
                const float warm_tangent_length_sq = glm::dot(warm_tangent_velocity, warm_tangent_velocity);
                if (warm_tangent_length_sq > 1e-8f) {
                    const glm::vec3 warm_tangent = warm_tangent_velocity / std::sqrt(warm_tangent_length_sq);
                    const glm::vec3 warm_tangent_impulse = warm_tangent * initial_manifold.points[0].tangent_impulse_accumulated;
                    apply_impulse(first_body, second_body, first_dynamic, second_dynamic, first_inverse_mass, second_inverse_mass, warm_tangent_impulse);
                }
            }

            const glm::vec3 first_velocity = first_body ? first_body->get_velocity() : glm::vec3(0.0f);
            const glm::vec3 second_velocity = second_body ? second_body->get_velocity() : glm::vec3(0.0f);
            const glm::vec3 relative_velocity = second_velocity - first_velocity;
            const float velocity_along_normal = glm::dot(relative_velocity, contact.normal);

            if (velocity_along_normal < 0.0f) {
                const float normal_impulse_magnitude = -(1.0f + restitution) * velocity_along_normal / inverse_mass_sum;
                const glm::vec3 normal_impulse = contact.normal * normal_impulse_magnitude;
                apply_impulse(first_body, second_body, first_dynamic, second_dynamic, first_inverse_mass, second_inverse_mass, normal_impulse);
                if (initial_manifold.point_count > 0u)
                    initial_manifold.points[0].normal_impulse_accumulated = std::max(0.0f, initial_manifold.points[0].normal_impulse_accumulated + normal_impulse_magnitude);

                const glm::vec3 resolved_first_velocity = first_body ? first_body->get_velocity() : glm::vec3(0.0f);
                const glm::vec3 resolved_second_velocity = second_body ? second_body->get_velocity() : glm::vec3(0.0f);
                const glm::vec3 resolved_relative_velocity = resolved_second_velocity - resolved_first_velocity;
                const glm::vec3 tangent_velocity = resolved_relative_velocity - glm::dot(resolved_relative_velocity, contact.normal) * contact.normal;
                const float tangent_length_sq = glm::dot(tangent_velocity, tangent_velocity);

                if (tangent_length_sq > 1e-8f) {
                    const glm::vec3 tangent = tangent_velocity / std::sqrt(tangent_length_sq);
                    const float tangent_impulse_magnitude = -glm::dot(resolved_relative_velocity, tangent) / inverse_mass_sum;

                    glm::vec3 friction_impulse;
                    const float max_static_friction = normal_impulse_magnitude * static_friction;
                    if (std::abs(tangent_impulse_magnitude) <= max_static_friction)
                        friction_impulse = tangent * tangent_impulse_magnitude;
                    else
                        friction_impulse = tangent * (-normal_impulse_magnitude * dynamic_friction);
                    apply_impulse(first_body, second_body, first_dynamic, second_dynamic, first_inverse_mass, second_inverse_mass, friction_impulse);
                    if (initial_manifold.point_count > 0u)
                        initial_manifold.points[0].tangent_impulse_accumulated += glm::dot(friction_impulse, tangent);
                }

                resolved_any = true;
                resolved_this_iteration = true;
            }

            const float corrected_penetration = std::max(contact.penetration_depth - penetration_slop, 0.0f);
            const glm::vec3 correction = contact.normal * (corrected_penetration * correction_percent / inverse_mass_sum);
            const glm::vec3 first_delta = -correction * first_inverse_mass;
            const glm::vec3 second_delta = correction * second_inverse_mass;

            auto resolve_body = [&current_positions, &resolved_any, &resolved_this_iteration](rigid_body* body, const glm::vec3& delta) {
                if (!body)
                    return;

                const bool has_position_delta = glm::dot(delta, delta) > 0.0f;
                if (!has_position_delta)
                    return;

                const glm::vec3 resolved_position = body->get_position() + delta;
                body->set_position(resolved_position);

                if (auto* node = body->get_node()) {
                    node->set_global_position(resolved_position);
                    current_positions[node->get_id()] = resolved_position;
                }

                resolved_any = true;
                resolved_this_iteration = true;
            };

            resolve_body(first_body, first_delta);
            resolve_body(second_body, second_delta);
        }

        if (!resolved_this_iteration)
            break;
    }

    return resolved_any;
}
