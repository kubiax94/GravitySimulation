#include "physics_system.h"
#include <algorithm>
#include <cmath>
#include <glm/gtx/string_cast.hpp>

#include "collider.h"
#include "frame_profiler.h"

std::vector<physics_data> physics_system::get_physics_data(const std::vector<rigid_body*>& bodies) const {
    std::vector<physics_data> data;
    data.reserve(bodies.size());

    for (auto* body : bodies) {
        if (body && body->get_compute_data())
            data.push_back(*body->get_compute_data());
    }

    return data;
}

std::vector<collision_data> physics_system::get_collision_data(const std::vector<rigid_body*>& bodies) const {
    std::vector<collision_data> data;
    data.reserve(bodies.size());

    for (size_t i = 0; i < bodies.size(); ++i) {
        auto* body = bodies[i];
        collision_data entry;
        entry.metadata.x = static_cast<uint32_t>(i);

        if (!body || !body->get_node() || !body->get_compute_data()) {
            data.push_back(entry);
            continue;
        }

        auto* node = body->get_node();
        auto* collider_component = node->find_component<collider>();
        if (!collider_component) {
            data.push_back(entry);
            continue;
        }

        const bounding_box world_bounds = collider_component->get_world_bounds();
        if (!world_bounds.valid) {
            data.push_back(entry);
            continue;
        }

        entry.center = glm::vec4(world_bounds.get_center(), 1.0f);
        entry.half_extents = glm::vec4(world_bounds.get_size() * 0.5f, 0.0f);
        entry.metadata.y = node->get_collision_layer_mask();
        entry.metadata.z = node->get_collision_query_mask();

        uint32_t flags = 0u;
        if (collider_component->is_enabled())
            flags |= collision_flag_enabled;
        if (collider_component->is_trigger())
            flags |= collision_flag_trigger;
        if (body->get_mass() > 0.0f)
            flags |= collision_flag_dynamic;
        entry.metadata.w = flags;
        data.push_back(entry);
    }

    return data;
}

void physics_system::rebuild_order_indices() {
    order_index_.clear();
    order_index_.reserve(order_id_.size());
    for (size_t i = 0; i < order_id_.size(); ++i)
        order_index_[order_id_[i]] = i;
}

void physics_system::sync_gpu_buffer(compute_shader* compute, const std::vector<rigid_body*>& bodies) {
    if (!compute || !compute->is_vaild() || bodies.empty())
        return;

    auto gpu_data = get_physics_data(bodies);
    compute->update_ssbo(0, gpu_data);
}

void physics_system::sync_collision_buffer(compute_shader* compute, const std::vector<rigid_body*>& bodies) {
    if (!compute || !compute->is_vaild() || bodies.empty())
        return;

    auto gpu_data = get_collision_data(bodies);
    compute->update_ssbo(1, gpu_data);
}

void physics_system::sync_collision_contact_buffer(compute_shader* compute, const std::vector<rigid_body*>& bodies) {
    if (!compute || !compute->is_vaild() || bodies.empty())
        return;

    std::vector<collision_contact_data> gpu_data(bodies.size());
    compute->update_ssbo(2, gpu_data);
}

void physics_system::apply_gpu_results_to_bodies(const std::vector<rigid_body*>& bodies, const std::vector<physics_data>& gpu_result) {
    const size_t count = std::min(bodies.size(), gpu_result.size());
    for (size_t i = 0; i < count; ++i) {
        auto* body = bodies[i];
        if (!body || !body->get_node() || !body->get_compute_data())
            continue;

        *body->get_compute_data() = gpu_result[i];
        body->get_node()->set_global_position(gpu_result[i].position);
        current_positions_[body->get_node()->get_id()] = gpu_result[i].position;
    }
}

void physics_system::sync_collision_contacts_from_gpu_data(const std::vector<collision_contact_data>& gpu_contacts, const std::vector<rigid_body*>& bodies) {
    if (bodies.empty()) {
        collision_pairs_.clear();
        solid_collision_contacts_.clear();
        return;
    }

    collision_pairs_.clear();
    solid_collision_contacts_.clear();
    collision_pairs_.reserve(gpu_contacts.size());
   solid_collision_contacts_.reserve(gpu_contacts.size() * collision_contact_capacity);
    std::unordered_map<collision_pair_key, size_t, collision_pair_key_hash> pair_indices;
    std::unordered_map<collision_pair_key, size_t, collision_pair_key_hash> solid_contact_indices;
    pair_indices.reserve(gpu_contacts.size() * collision_contact_capacity);
    solid_contact_indices.reserve(gpu_contacts.size() * collision_contact_capacity);

    for (const auto& contact_data : gpu_contacts) {
        for (uint32_t slot = 0; slot < collision_contact_capacity; ++slot) {
            if ((contact_data.metadata[slot].z & collision_contact_flag_active) == 0u)
                continue;

            const uint32_t first_index = contact_data.metadata[slot].x;
            const uint32_t second_index = contact_data.metadata[slot].y;
            if (first_index >= bodies.size() || second_index >= bodies.size())
                continue;

            auto* first_body = bodies[first_index];
            auto* second_body = bodies[second_index];
            if (!first_body || !second_body || !first_body->get_node() || !second_body->get_node())
                continue;

            auto* first = first_body->get_node()->find_component<collider>();
            auto* second = second_body->get_node()->find_component<collider>();
            if (!first || !second)
                continue;

            bounding_box overlap_bounds;
            if (!intersects(*first, *second, &overlap_bounds))
                continue;

          const collision_pair_key pair_key = make_collision_pair_key(first, second);
            auto pair_index_it = pair_indices.find(pair_key);
            if (pair_index_it == pair_indices.end()) {
                pair_indices.emplace(pair_key, collision_pairs_.size());
                collision_pairs_.push_back({ first, second, overlap_bounds });
            }
            else {
                auto& pair = collision_pairs_[pair_index_it->second];
                const glm::vec3 current_size = pair.overlap_bounds.get_size();
                const glm::vec3 next_size = overlap_bounds.get_size();
                const float current_volume = current_size.x * current_size.y * current_size.z;
                const float next_volume = next_size.x * next_size.y * next_size.z;
                if (!pair.overlap_bounds.valid || next_volume > current_volume)
                    pair.overlap_bounds = overlap_bounds;
            }

            if ((contact_data.metadata[slot].z & collision_contact_flag_trigger) != 0u)
                continue;

            solid_collision_contact contact;
            contact.first = first;
            contact.second = second;
            contact.overlap_bounds = overlap_bounds;
            contact.normal = glm::vec3(contact_data.normal_penetration[slot]);
            contact.penetration_depth = contact_data.normal_penetration[slot].w;
            if (!contact.is_valid())
                continue;

            auto solid_contact_it = solid_contact_indices.find(pair_key);
            if (solid_contact_it == solid_contact_indices.end()) {
                solid_contact_indices.emplace(pair_key, solid_collision_contacts_.size());
                solid_collision_contacts_.push_back(contact);
            }
            else if (contact.penetration_depth > solid_collision_contacts_[solid_contact_it->second].penetration_depth) {
                solid_collision_contacts_[solid_contact_it->second] = contact;
            }
        }
    }
}

bool physics_system::requires_cpu_collision_stage(const std::vector<rigid_body*>& bodies) const {
    std::unordered_set<uuid> body_node_ids;
    body_node_ids.reserve(bodies.size());

    for (const auto* body : bodies) {
        if (!body || !body->get_node())
            continue;

        body_node_ids.insert(body->get_node()->get_id());
    }

    for (const auto* collider_component : colliders_) {
        if (!collider_component || !collider_component->is_enabled() || !collider_component->get_node())
            continue;

        if (!body_node_ids.contains(collider_component->get_node()->get_id()))
            return true;
    }

    return false;
}

collision_pair_key physics_system::make_collision_pair_key(collider* first, collider* second) {
    if (std::less<collider*>{}(second, first))
        std::swap(first, second);

    return { first, second };
}

collision_event physics_system::make_collision_event(collider* self, collider* other, const bounding_box& overlap_bounds) {
    collision_event event;
    event.self = self;
    event.other = other;
    event.overlap_bounds = overlap_bounds;
    event.is_trigger_interaction = (self && self->is_trigger()) || (other && other->is_trigger());
    return event;
}

solid_collision_contact physics_system::make_solid_collision_contact(const collision_pair& pair) {
    solid_collision_contact contact;
    contact.first = pair.first;
    contact.second = pair.second;
    contact.overlap_bounds = pair.overlap_bounds;

    if (!pair.is_valid() || !pair.first || !pair.second || pair.first->is_trigger() || pair.second->is_trigger())
        return contact;

    const glm::vec3 overlap_size = pair.overlap_bounds.get_size();
    if (overlap_size.x <= 0.0f || overlap_size.y <= 0.0f || overlap_size.z <= 0.0f)
        return contact;

    const glm::vec3 first_center = pair.first->get_world_bounds().get_center();
    const glm::vec3 second_center = pair.second->get_world_bounds().get_center();
    const glm::vec3 delta = second_center - first_center;

    contact.penetration_depth = overlap_size.x;
    contact.normal = glm::vec3(delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);

    if (overlap_size.y < contact.penetration_depth) {
        contact.penetration_depth = overlap_size.y;
        contact.normal = glm::vec3(0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
    }

    if (overlap_size.z < contact.penetration_depth) {
        contact.penetration_depth = overlap_size.z;
        contact.normal = glm::vec3(0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f);
    }

    return contact;
}

void physics_system::update_collision_pairs() {
    collision_pairs_.clear();

    for (size_t i = 0; i < colliders_.size(); ++i) {
        auto* first = colliders_[i];
        if (!first || !first->is_enabled() || !first->get_node())
            continue;

        for (size_t j = i + 1; j < colliders_.size(); ++j) {
            auto* second = colliders_[j];
            if (!second || !second->is_enabled() || !second->get_node())
                continue;

            if (first->get_node() == second->get_node())
                continue;

            const auto first_layer = first->get_node()->get_collision_layer_mask();
            const auto first_query = first->get_node()->get_collision_query_mask();
            const auto second_layer = second->get_node()->get_collision_layer_mask();
            const auto second_query = second->get_node()->get_collision_query_mask();
            if (!collision_pair_matches(first_layer, first_query, second_layer, second_query))
                continue;

            bounding_box overlap_bounds;
            if (!intersects(*first, *second, &overlap_bounds))
                continue;

            collision_pairs_.push_back({ first, second, overlap_bounds });
        }
    }
}

void physics_system::update_collision_events() {
    collision_enter_events_.clear();
    collision_stay_events_.clear();
    collision_exit_events_.clear();

    std::unordered_map<collision_pair_key, collision_event, collision_pair_key_hash> current_collision_events;
    current_collision_events.reserve(collision_pairs_.size());

    for (const auto& pair : collision_pairs_) {
        if (!pair.is_valid())
            continue;

        const collision_pair_key key = make_collision_pair_key(pair.first, pair.second);
        const collision_event event = make_collision_event(pair.first, pair.second, pair.overlap_bounds);
        current_collision_events.insert_or_assign(key, event);

        if (previous_collision_events_.contains(key))
            collision_stay_events_.push_back(event);
        else
            collision_enter_events_.push_back(event);
    }

    for (const auto& [key, previous_event] : previous_collision_events_) {
        if (!current_collision_events.contains(key))
            collision_exit_events_.push_back(previous_event);
    }

    previous_collision_events_ = std::move(current_collision_events);
}

void physics_system::update_solid_collision_contacts() {
    solid_collision_contacts_.clear();
    solid_collision_contacts_.reserve(collision_pairs_.size());

    for (const auto& pair : collision_pairs_) {
        const solid_collision_contact contact = make_solid_collision_contact(pair);
        if (contact.is_valid())
            solid_collision_contacts_.push_back(contact);
    }
}

void physics_system::resolve_solid_collisions() {
    bool resolved_any = false;
    constexpr float penetration_slop = 0.0005f;
    constexpr float correction_percent = 0.7f;
    constexpr float minimum_mass = 0.0001f;
    constexpr float restitution = 0.0f;
    constexpr float static_friction = 0.5f;
    constexpr float dynamic_friction = 0.35f;

    for (int iteration = 0; iteration < contact_solver_iterations_; ++iteration) {
        bool resolved_this_iteration = false;

        for (const auto& initial_contact : solid_collision_contacts_) {
            if (!initial_contact.first || !initial_contact.second)
                continue;

            bounding_box overlap_bounds;
            if (!intersects(*initial_contact.first, *initial_contact.second, &overlap_bounds))
                continue;

            const solid_collision_contact contact = make_solid_collision_contact({ initial_contact.first, initial_contact.second, overlap_bounds });
            if (!contact.is_valid())
                continue;

            auto* first_node = contact.first->get_node();
            auto* second_node = contact.second->get_node();
            if (!first_node || !second_node)
                continue;

            auto first_body_it = rigid_bodies_by_node_id_.find(first_node->get_id());
            auto second_body_it = rigid_bodies_by_node_id_.find(second_node->get_id());
            rigid_body* first_body = first_body_it != rigid_bodies_by_node_id_.end() ? first_body_it->second : nullptr;
            rigid_body* second_body = second_body_it != rigid_bodies_by_node_id_.end() ? second_body_it->second : nullptr;
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

            const glm::vec3 first_velocity = first_body ? first_body->get_velocity() : glm::vec3(0.0f);
            const glm::vec3 second_velocity = second_body ? second_body->get_velocity() : glm::vec3(0.0f);
            const glm::vec3 relative_velocity = second_velocity - first_velocity;
            const float velocity_along_normal = glm::dot(relative_velocity, contact.normal);

            if (velocity_along_normal < 0.0f) {
                const float normal_impulse_magnitude = -(1.0f + restitution) * velocity_along_normal / inverse_mass_sum;
                const glm::vec3 normal_impulse = contact.normal * normal_impulse_magnitude;

                if (first_dynamic)
                    first_body->set_velocity(first_velocity - normal_impulse * first_inverse_mass);

                if (second_dynamic)
                    second_body->set_velocity(second_velocity + normal_impulse * second_inverse_mass);

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

                    if (first_dynamic)
                        first_body->set_velocity(first_body->get_velocity() - friction_impulse * first_inverse_mass);

                    if (second_dynamic)
                        second_body->set_velocity(second_body->get_velocity() + friction_impulse * second_inverse_mass);
                }

                resolved_any = true;
                resolved_this_iteration = true;
            }

            const float corrected_penetration = std::max(contact.penetration_depth - penetration_slop, 0.0f);
            const glm::vec3 correction = contact.normal * (corrected_penetration * correction_percent / inverse_mass_sum);
            const glm::vec3 first_delta = -correction * first_inverse_mass;
            const glm::vec3 second_delta = correction * second_inverse_mass;

            auto resolve_body = [this, &resolved_any, &resolved_this_iteration](rigid_body* body, const glm::vec3& delta) {
                if (!body)
                    return;

                const bool has_position_delta = glm::dot(delta, delta) > 0.0f;
                if (!has_position_delta)
                    return;

                const glm::vec3 resolved_position = body->get_position() + delta;
                body->set_position(resolved_position);

                if (auto* node = body->get_node()) {
                    node->set_global_position(resolved_position);
                    current_positions_[node->get_id()] = resolved_position;
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

    gpu_buffer_dirty_ = gpu_buffer_dirty_ || resolved_any;
}

void physics_system::dispatch_collision_events(const std::vector<collision_event>& events, void (scene_node::*handler)(const collision_event&)) {
    for (const auto& event : events) {
        if (!event.is_valid())
            continue;

        auto* self_node = event.self ? event.self->get_node() : nullptr;
        auto* other_node = event.other ? event.other->get_node() : nullptr;
        if (!self_node || !other_node)
            continue;

        (self_node->*handler)(event);
        (other_node->*handler)(invert_collision_event(event));
    }
}

void physics_system::remove_collision_state(collider* collider_component) {
    if (!collider_component)
        return;

    auto remove_from_events = [collider_component](std::vector<collision_event>& events) {
        events.erase(std::remove_if(events.begin(), events.end(), [collider_component](const collision_event& event) {
            return event.self == collider_component || event.other == collider_component;
        }), events.end());
    };

    remove_from_events(collision_enter_events_);
    remove_from_events(collision_stay_events_);
    remove_from_events(collision_exit_events_);

    for (auto it = previous_collision_events_.begin(); it != previous_collision_events_.end();) {
        if (it->second.self == collider_component || it->second.other == collider_component)
            it = previous_collision_events_.erase(it);
        else
            ++it;
    }
}

physics_system::physics_system(unit_system* u_sys) : u_sys_(u_sys) {
    auto* gravity_comp = new compute_shader("gravity_simulation.glsl");
    gravity_simulation_comp_ = gravity_comp;
    default_compute_shader_id_ = gravity_comp->get_id();
    register_in(gravity_comp, physics_gpu_stage::gravity_integration);
    gravity_comp->add_ssbo(0, std::vector<physics_data>{});
   gravity_comp->add_ssbo(1, std::vector<collision_data>{});

    collision_detect_comp_ = new compute_shader("collision_detect.glsl");
    register_in(collision_detect_comp_, physics_gpu_stage::collision_detection);
    collision_detect_comp_->add_ssbo(0, std::vector<physics_data>{});
    collision_detect_comp_->add_ssbo(1, std::vector<collision_data>{});
    collision_detect_comp_->add_ssbo(2, std::vector<collision_contact_data>{});

    collision_resolve_comp_ = new compute_shader("collision_resolve.glsl");
    register_in(collision_resolve_comp_, physics_gpu_stage::collision_resolution);
    collision_resolve_comp_->add_ssbo(0, std::vector<physics_data>{});
    collision_resolve_comp_->add_ssbo(1, std::vector<collision_data>{});
    collision_resolve_comp_->add_ssbo(2, std::vector<collision_contact_data>{});
}

bool physics_system::add(rigid_body* r_body) {
    auto* node = r_body->get_node();
    const uuid node_id = node->get_id();

    if (entities_.contains(node_id))
        return false;
    if (!compute_shaders_.contains(r_body->get_compute_shader_id()) && gravity_simulation_comp_)
        r_body->set_compute_shader(gravity_simulation_comp_->get_id());

    entities_.emplace(node_id, r_body);
    order_id_.push_back(node_id);
    order_index_[node_id] = order_id_.size() - 1;
    previous_positions_[node_id] = r_body->get_position();
    current_positions_[node_id] = r_body->get_position();
    rigid_bodies_by_node_id_[node_id] = r_body;
    compute_groups_[r_body->get_compute_shader_id()].push_back(r_body);
    gpu_buffer_dirty_ = true;

    return true;
}

bool physics_system::add(collider* collider_component) {
    if (!collider_component)
        return false;

    if (std::ranges::find(colliders_, collider_component) != colliders_.end())
        return false;

    colliders_.push_back(collider_component);
    return true;
}

void physics_system::compute_cpu() {
    std::vector<std::pair<uuid, rigid_body*>> all(entities_.begin(), entities_.end());

    for (size_t i = 0; i < all.size(); ++i) {
        auto [a_node, a_body] = all[i];
        for (size_t j = i + 1; j < all.size(); ++j) {
            auto [b_node, b_body] = all[j];

            glm::vec3 dir = b_body->get_position() - a_body->get_position();
            float dist = glm::length(dir);

            if (dist < 1e-3f)
                continue;

            glm::vec3 force_dir = glm::normalize(dir);
            float force_mag = u_sys_->scaled_G() * a_body->get_mass() * b_body->get_mass() / (dist * dist);
            glm::vec3 force = force_dir * force_mag;

            a_body->apply_force(force);
            b_body->apply_force(-force);
        }
    }
}

void physics_system::run_default_gpu_pipeline(const float& dt) {
    if (!gravity_simulation_comp_ || !gravity_simulation_comp_->is_vaild())
        return;

    auto group_it = compute_groups_.find(default_compute_shader_id_);
    if (group_it == compute_groups_.end() || group_it->second.empty())
        return;

    auto& bodies = group_it->second;
    const bool requires_cpu_readback = std::ranges::any_of(bodies, [this](const rigid_body* body) {
        return !body || !body->get_node() || !gpu_driven_nodes_.contains(body->get_node()->get_id());
    });

    {
        auto section = frame_profiler::measure_active("fixed_update_gpu_default_upload_ssbo");
        if (gpu_buffer_dirty_) {
            sync_gpu_buffer(gravity_simulation_comp_, bodies);
          sync_collision_buffer(gravity_simulation_comp_, bodies);
            readback_pending_[default_compute_shader_id_] = false;
        }
    }

    readback_pending_[default_compute_shader_id_] = false;

    const GLuint groups_x = static_cast<GLuint>((bodies.size() + 64u - 1u) / 64u);
    {
        auto section = frame_profiler::measure_active("fixed_update_gpu_default_dispatch_gravity");
        gravity_simulation_comp_->use();
        gravity_simulation_comp_->set_uni_float("G", u_sys_->scaled_G());
        gravity_simulation_comp_->set_uni_float("dt", u_sys_->time(dt) * simulation_speed_);
        gravity_simulation_comp_->set_uni_float("rawDt", dt);
        gravity_simulation_comp_->set_uni_float("simulationTime", simulation_time_);
        gravity_simulation_comp_->dispatch({ groups_x, 1, 1 });
    }

    if (requires_cpu_readback) {
        auto section = frame_profiler::measure_active("fixed_update_gpu_default_readback_sync");
        std::vector<physics_data> gpu_result;
        gravity_simulation_comp_->get_binding_data(0, gpu_result);
        apply_gpu_results_to_bodies(bodies, gpu_result);
    }

    run_collision_stage_gpu(bodies);

    {
        auto section = frame_profiler::measure_active("fixed_update_gpu_default_upload_post_collision");
        sync_gpu_buffer(gravity_simulation_comp_, bodies);
       sync_collision_buffer(gravity_simulation_comp_, bodies);
    }

}

void physics_system::run_custom_gpu_groups(const float& dt) {
    std::unordered_set<uuid> processed_shader_ids;
    processed_shader_ids.insert(default_compute_shader_id_);

    for (auto& [shader_id, bodies] : compute_groups_) {
        if (processed_shader_ids.contains(shader_id))
            continue;

        auto shader_it = compute_shaders_.find(shader_id);
        if (shader_it == compute_shaders_.end() || !shader_it->second || !shader_it->second->is_vaild() || bodies.empty())
            continue;

        auto stage_it = compute_shader_stages_.find(shader_id);
        if (stage_it != compute_shader_stages_.end() && stage_it->second != physics_gpu_stage::custom)
            continue;

        auto* compute = shader_it->second;
        const bool requires_cpu_readback = std::ranges::any_of(bodies, [this](const rigid_body* body) {
            return !body || !body->get_node() || !gpu_driven_nodes_.contains(body->get_node()->get_id());
        });

        {
            auto section = frame_profiler::measure_active("fixed_update_gpu_upload_ssbo");
            if (gpu_buffer_dirty_) {
                sync_gpu_buffer(compute, bodies);
                readback_pending_[shader_id] = false;
            }
        }

        const GLuint groups_x = static_cast<GLuint>((bodies.size() + 64u - 1u) / 64u);
        {
            auto section = frame_profiler::measure_active("fixed_update_gpu_dispatch");
            compute->use();
            compute->set_uni_float("G", u_sys_->scaled_G());
            compute->set_uni_float("dt", u_sys_->time(dt) * simulation_speed_);
            compute->set_uni_float("rawDt", dt);
            compute->set_uni_float("simulationTime", simulation_time_);
            compute->dispatch({ groups_x, 1, 1 });
        }

        if (requires_cpu_readback) {
            auto section = frame_profiler::measure_active("fixed_update_gpu_apply_result_sync");
            std::vector<physics_data> gpu_result;
            compute->get_binding_data(0, gpu_result);
            apply_gpu_results_to_bodies(bodies, gpu_result);
        }
    }
}

void physics_system::run_collision_stage_gpu(const std::vector<rigid_body*>& bodies) {
  if (!collision_detect_comp_ || !collision_detect_comp_->is_vaild() || bodies.empty() || requires_cpu_collision_stage(bodies)) {
        run_collision_stage_cpu();
        return;
    }

    {
        auto section = frame_profiler::measure_active("fixed_update_gpu_collision_upload");
        sync_gpu_buffer(collision_detect_comp_, bodies);
        sync_collision_buffer(collision_detect_comp_, bodies);
        sync_collision_contact_buffer(collision_detect_comp_, bodies);
    }

    const GLuint groups_x = static_cast<GLuint>((bodies.size() + 64u - 1u) / 64u);
    {
        auto section = frame_profiler::measure_active("fixed_update_gpu_collision_detect");
        collision_detect_comp_->use();
        collision_detect_comp_->dispatch({ groups_x, 1, 1 });
    }

    std::vector<collision_contact_data> gpu_contacts;
    collision_detect_comp_->get_binding_data(2, gpu_contacts);
    sync_collision_contacts_from_gpu_data(gpu_contacts, bodies);
    resolve_solid_collisions();
    update_collision_pairs();
    update_solid_collision_contacts();
}

void physics_system::run_collision_stage_cpu() {
   constexpr int resolve_iterations = 4;
    for (int iteration = 0; iteration < resolve_iterations; ++iteration) {
        update_collision_pairs();
        update_solid_collision_contacts();
        if (solid_collision_contacts_.empty())
            break;
        resolve_solid_collisions();
    }

    update_collision_pairs();
    update_solid_collision_contacts();
}

void physics_system::compute_gpu(const float& dt) {
   auto default_group_it = compute_groups_.find(default_compute_shader_id_);
    const bool has_default_group = default_group_it != compute_groups_.end() && !default_group_it->second.empty();

    run_default_gpu_pipeline(dt);
    run_custom_gpu_groups(dt);

    if (!has_default_group) {
        run_collision_stage_cpu();

        for (auto& [shader_id, bodies] : compute_groups_) {
            auto shader_it = compute_shaders_.find(shader_id);
            if (shader_it == compute_shaders_.end() || !shader_it->second || !shader_it->second->is_vaild() || bodies.empty())
                continue;

            auto stage_it = compute_shader_stages_.find(shader_id);
            if (stage_it != compute_shader_stages_.end() && stage_it->second != physics_gpu_stage::custom)
                continue;

            sync_gpu_buffer(shader_it->second, bodies);
        }
    }

    gpu_buffer_dirty_ = false;
}

bool physics_system::remove(rigid_body* rigid_body) {
    uuid node_id = rigid_body->get_node()->get_id();

    if (entities_.contains(node_id)) {
        auto group_it = compute_groups_.find(rigid_body->get_compute_shader_id());
        if (group_it != compute_groups_.end()) {
            auto& group = group_it->second;
            group.erase(std::remove(group.begin(), group.end(), rigid_body), group.end());
        }

        entities_.erase(node_id);
        order_id_.erase(std::remove(order_id_.begin(), order_id_.end(), node_id), order_id_.end());
        rebuild_order_indices();
        previous_positions_.erase(node_id);
        current_positions_.erase(node_id);
        rigid_bodies_by_node_id_.erase(node_id);
        gpu_buffer_dirty_ = true;

        return true;
    }

    return false;
}

bool physics_system::remove(collider* collider_component) {
    if (!collider_component)
        return false;

    const auto it = std::remove(colliders_.begin(), colliders_.end(), collider_component);
    if (it == colliders_.end())
        return false;

    colliders_.erase(it, colliders_.end());
    collision_pairs_.erase(std::remove_if(collision_pairs_.begin(), collision_pairs_.end(), [collider_component](const collision_pair& pair) {
        return pair.first == collider_component || pair.second == collider_component;
    }), collision_pairs_.end());
    solid_collision_contacts_.erase(std::remove_if(solid_collision_contacts_.begin(), solid_collision_contacts_.end(), [collider_component](const solid_collision_contact& contact) {
        return contact.first == collider_component || contact.second == collider_component;
    }), solid_collision_contacts_.end());
    remove_collision_state(collider_component);
    return true;
}

const std::vector<collider*>& physics_system::get_colliders() const {
    return colliders_;
}

const std::vector<collision_pair>& physics_system::get_collision_pairs() const {
    return collision_pairs_;
}

const std::vector<solid_collision_contact>& physics_system::get_solid_collision_contacts() const {
    return solid_collision_contacts_;
}

const std::vector<collision_event>& physics_system::get_collision_enter_events() const {
    return collision_enter_events_;
}

const std::vector<collision_event>& physics_system::get_collision_stay_events() const {
    return collision_stay_events_;
}

const std::vector<collision_event>& physics_system::get_collision_exit_events() const {
    return collision_exit_events_;
}

void physics_system::register_in(compute_shader* c_shader) {
  register_in(c_shader, physics_gpu_stage::custom);
}

void physics_system::register_in(compute_shader* c_shader, physics_gpu_stage stage) {
    if (!c_shader)
        return;
    compute_shaders_[c_shader->get_id()] = c_shader;
   compute_shader_stages_[c_shader->get_id()] = stage;
}

void physics_system::set_simulation_speed(float speed) {
    simulation_speed_ = speed > 0.f ? speed : 1.f;
}

float physics_system::get_simulation_speed() const {
    return simulation_speed_;
}

void physics_system::set_gpu_driven_nodes(const std::unordered_set<uuid>& node_ids) {
    gpu_driven_nodes_ = node_ids;
}

void physics_system::sync_scene_positions(float alpha) const {
    auto section = frame_profiler::measure_active("scene_sync_render_apply_positions");
    for (const auto& id : order_id_) {
        if (gpu_driven_nodes_.contains(id))
            continue;

        auto entityIt = entities_.find(id);
        auto prevIt = previous_positions_.find(id);
        auto currIt = current_positions_.find(id);
        if (entityIt == entities_.end() || prevIt == previous_positions_.end() || currIt == current_positions_.end())
            continue;

        auto* node = entityIt->second->get_node();
        if (!node)
            continue;

        const glm::vec3 position = glm::mix(prevIt->second, currIt->second, alpha);
        node->set_global_position(position);
    }
}

void physics_system::update(const float& dt) {
    const float scaled_dt = u_sys_->time(dt) * simulation_speed_;
    simulation_time_ += scaled_dt;

    for (const auto& id : order_id_) {
        auto it = entities_.find(id);
        if (it == entities_.end() || !it->second || !it->second->get_node())
            continue;

        previous_positions_[id] = current_positions_[id];
    }

    bool has_valid_gpu_group = false;
    for (const auto& [shader_id, bodies] : compute_groups_) {
        auto shader_it = compute_shaders_.find(shader_id);
        if (shader_it != compute_shaders_.end() && shader_it->second && shader_it->second->is_vaild() && !bodies.empty()) {
            has_valid_gpu_group = true;
            break;
        }
    }

    if (has_valid_gpu_group) {
        auto section = frame_profiler::measure_active("fixed_update_gpu_total");
        compute_gpu(dt);
    }
    else {
        auto section = frame_profiler::measure_active("fixed_update_cpu_fallback");
        compute_cpu();
        for (auto& entity : entities_ | std::views::values) {
            const auto id = entity->get_node()->get_id();
            entity->integrate(scaled_dt);
            current_positions_[id] = entity->get_position();
        }

        update_collision_pairs();
        update_solid_collision_contacts();
        resolve_solid_collisions();
    }

    update_collision_events();
    dispatch_collision_events(collision_enter_events_, &scene_node::on_collision_enter);
    dispatch_collision_events(collision_stay_events_, &scene_node::on_collision_stay);
    dispatch_collision_events(collision_exit_events_, &scene_node::on_collision_exit);

    ++frame_idx_;
}

size_t physics_system::get_body_index(const uuid& id) const {
    auto it = order_index_.find(id);
    return it != order_index_.end() ? it->second : static_cast<size_t>(-1);
}

GLuint physics_system::get_render_ssbo() const {
    compute_shader* active_shader = nullptr;
    for (const auto& [shader_id, bodies] : compute_groups_) {
        if (bodies.empty())
            continue;

        auto shader_it = compute_shaders_.find(shader_id);
        if (shader_it == compute_shaders_.end() || !shader_it->second || !shader_it->second->is_vaild())
            continue;

        if (active_shader && active_shader != shader_it->second)
            return 0;

        active_shader = shader_it->second;
    }

    return active_shader ? active_shader->get_ssbo_id(0) : 0;
}
