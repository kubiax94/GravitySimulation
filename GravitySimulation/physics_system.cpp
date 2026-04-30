#include "physics_system.h"
#include <algorithm>
#include <cmath>
#include <glm/gtx/string_cast.hpp>

#include "broadphase_pair.h"
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
        contact_manifolds_.clear();
        solid_collision_contacts_.clear();
        return;
    }

    collision_pairs_.clear();
    contact_manifolds_.clear();
    solid_collision_contacts_.clear();
    collision_pairs_.reserve(gpu_contacts.size());
    contact_manifolds_.reserve(gpu_contacts.size() * collision_contact_capacity);
   solid_collision_contacts_.reserve(gpu_contacts.size() * collision_contact_capacity);
    std::unordered_map<collision_pair_key, size_t, collision_pair_key_hash> pair_indices;
    std::unordered_map<collision_pair_key, size_t, collision_pair_key_hash> manifold_indices;
    std::unordered_map<collision_pair_key, size_t, collision_pair_key_hash> solid_contact_indices;
    pair_indices.reserve(gpu_contacts.size() * collision_contact_capacity);
    manifold_indices.reserve(gpu_contacts.size() * collision_contact_capacity);
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

            contact_manifold manifold;
            manifold.first = first;
            manifold.second = second;
            manifold.overlap_bounds = overlap_bounds;
            manifold.normal = glm::vec3(contact_data.normal_penetration[slot]);
            manifold.point_count = 1u;
            manifold.points[0].position = overlap_bounds.get_center();
            manifold.points[0].penetration = contact_data.normal_penetration[slot].w;
            if (!manifold.is_valid())
                continue;

            auto manifold_it = manifold_indices.find(pair_key);
            if (manifold_it == manifold_indices.end()) {
                manifold_indices.emplace(pair_key, contact_manifolds_.size());
                contact_manifolds_.push_back(manifold);
            }
            else if (manifold.get_max_penetration() > contact_manifolds_[manifold_it->second].get_max_penetration()) {
                contact_manifolds_[manifold_it->second] = manifold;
            }

            const solid_collision_contact contact = make_solid_collision_contact(manifold);
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

solid_collision_contact physics_system::make_solid_collision_contact(const contact_manifold& manifold) {
    solid_collision_contact contact;
    contact.first = manifold.first;
    contact.second = manifold.second;
    contact.overlap_bounds = manifold.overlap_bounds;

    if (!manifold.is_valid() || !manifold.first || !manifold.second || manifold.is_trigger)
        return contact;

    contact.normal = manifold.normal;
    contact.penetration_depth = manifold.get_max_penetration();

    return contact;
}

void physics_system::update_collision_pairs() {
    collision_pairs_.clear();
    collision_candidate_pairs_.clear();

    const auto proxies = collision_broadphase_.build_proxies(colliders_);
    collision_candidate_pairs_ = collision_broadphase_.find_pairs(proxies);
    collision_pairs_.reserve(collision_candidate_pairs_.size());

    for (const auto& candidate_pair : collision_candidate_pairs_) {
        if (!candidate_pair.is_valid())
            continue;

        bounding_box overlap_bounds;
        if (!intersects(*candidate_pair.first, *candidate_pair.second, &overlap_bounds))
            continue;

        collision_pairs_.push_back({ candidate_pair.first, candidate_pair.second, overlap_bounds });
    }
}

void physics_system::update_contact_manifolds() {
    contact_manifolds_.clear();
    contact_manifolds_.reserve(collision_candidate_pairs_.size());

    for (const auto& candidate_pair : collision_candidate_pairs_) {
        if (!candidate_pair.is_valid())
            continue;

        contact_manifold manifold;
        if (!collision_narrowphase_.build_manifold(candidate_pair, manifold))
            continue;

        const auto cache_it = contact_manifold_cache_.find(make_contact_manifold_key(manifold.first, manifold.second));
        if (cache_it != contact_manifold_cache_.end()) {
            manifold.persistence = cache_it->second.persistence + 1u;
            const uint32_t copy_count = std::min(manifold.point_count, cache_it->second.point_count);
            for (uint32_t point_index = 0; point_index < copy_count; ++point_index) {
                manifold.points[point_index].normal_impulse_accumulated = cache_it->second.points[point_index].normal_impulse_accumulated;
                manifold.points[point_index].tangent_impulse_accumulated = cache_it->second.points[point_index].tangent_impulse_accumulated;
            }
        }
        else {
            manifold.persistence = 1u;
        }

        contact_manifolds_.push_back(manifold);
    }
}

void physics_system::sync_contact_manifold_cache() {
    std::unordered_map<contact_manifold_key, contact_manifold, contact_manifold_key_hash> next_cache;
    next_cache.reserve(contact_manifolds_.size());

    for (const auto& manifold : contact_manifolds_) {
        if (!manifold.first || !manifold.second)
            continue;

        next_cache.insert_or_assign(make_contact_manifold_key(manifold.first, manifold.second), manifold);
    }

    contact_manifold_cache_ = std::move(next_cache);
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
    solid_collision_contacts_.reserve(contact_manifolds_.size());

    for (const auto& manifold : contact_manifolds_) {
        const solid_collision_contact contact = make_solid_collision_contact(manifold);
        if (contact.is_valid())
            solid_collision_contacts_.push_back(contact);
    }
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
    gpu_buffer_dirty_ = gpu_buffer_dirty_ || collision_solver_.solve(contact_manifolds_, collision_narrowphase_, rigid_bodies_by_node_id_, current_positions_, contact_solver_iterations_);
    update_collision_pairs();
    update_contact_manifolds();
    update_solid_collision_contacts();
}

void physics_system::run_collision_stage_cpu() {
   constexpr int resolve_iterations = 4;
    for (int iteration = 0; iteration < resolve_iterations; ++iteration) {
        update_collision_pairs();
        update_contact_manifolds();
        update_solid_collision_contacts();
        if (solid_collision_contacts_.empty())
            break;
        gpu_buffer_dirty_ = gpu_buffer_dirty_ || collision_solver_.solve(contact_manifolds_, collision_narrowphase_, rigid_bodies_by_node_id_, current_positions_, contact_solver_iterations_);
    }

    update_collision_pairs();
    update_contact_manifolds();
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

            const bool requires_cpu_upload = std::ranges::any_of(bodies, [this](const rigid_body* body) {
                return !body || !body->get_node() || !gpu_driven_nodes_.contains(body->get_node()->get_id());
            });

            if (requires_cpu_upload)
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
    for (auto it = contact_manifold_cache_.begin(); it != contact_manifold_cache_.end();) {
        if (it->second.first == collider_component || it->second.second == collider_component)
            it = contact_manifold_cache_.erase(it);
        else
            ++it;
    }
    remove_collision_state(collider_component);
    return true;
}

const std::vector<collider*>& physics_system::get_colliders() const {
    return colliders_;
}

const std::vector<collision_pair>& physics_system::get_collision_pairs() const {
    return collision_pairs_;
}

const std::vector<contact_manifold>& physics_system::get_contact_manifolds() const {
    return contact_manifolds_;
}

const std::vector<solid_collision_contact>& physics_system::get_solid_collision_contacts() const {
    return solid_collision_contacts_;
}

size_t physics_system::get_cached_contact_manifold_count() const {
    return contact_manifold_cache_.size();
}

size_t physics_system::get_persistent_contact_manifold_count() const {
    return std::count_if(contact_manifolds_.begin(), contact_manifolds_.end(), [](const contact_manifold& manifold) {
        return manifold.persistence > 1u;
    });
}

size_t physics_system::get_warm_contact_point_count() const {
    size_t count = 0;

    for (const auto& manifold : contact_manifolds_) {
        for (uint32_t point_index = 0; point_index < manifold.point_count && point_index < manifold.points.size(); ++point_index) {
            const auto& point = manifold.points[point_index];
            if (std::abs(point.normal_impulse_accumulated) > 1e-5f || std::abs(point.tangent_impulse_accumulated) > 1e-5f)
                ++count;
        }
    }

    return count;
}

uint32_t physics_system::get_max_contact_persistence() const {
    uint32_t max_persistence = 0u;

    for (const auto& manifold : contact_manifolds_)
        max_persistence = std::max(max_persistence, manifold.persistence);

    return max_persistence;
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
        update_contact_manifolds();
        update_solid_collision_contacts();
        gpu_buffer_dirty_ = gpu_buffer_dirty_ || collision_solver_.solve(contact_manifolds_, collision_narrowphase_, rigid_bodies_by_node_id_, current_positions_, contact_solver_iterations_);
    }

    sync_contact_manifold_cache();

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
