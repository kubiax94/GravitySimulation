#include "physics_system.h"
#include <glm/gtx/string_cast.hpp>

#include "frame_profiler.h"

//float scale_mass = 1e24f; // masa Ziemi
//float scale_distance = 1e6f; // 1mln km
//float scale_time = 3.872e6f / 3600.f; // 1h

std::vector<physics_data> physics_system::get_physics_data(const std::vector<rigid_body*>& bodies) const {
    std::vector<physics_data> data;
    data.reserve(bodies.size());

    for (auto* body : bodies) {
		if (body && body->get_compute_data())
			data.push_back(*body->get_compute_data());
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

//FOR NOW IT'S CREATING BY DEFAULT COMPUTE SHADER WITH SSBO ATTACHED TO PHYCIS_DATA
physics_system::physics_system(unit_system* u_sys): u_sys_(u_sys) {
	auto* gravity_comp = new compute_shader("gravity_simulation.glsl");
	// store pointer to the default gravity compute shader so later calls use a valid object
	gravity_simulation_comp_ = gravity_comp;
	register_in(gravity_comp);
  gravity_comp->add_ssbo(0, std::vector<physics_data>{});
}

bool physics_system::add(rigid_body* r_body) {
	auto* node = r_body->get_node();
	const uuid node_id = node->get_id();

   if (entities_.contains(node_id)) return false;
 if (!compute_shaders_.contains(r_body->get_compute_shader_id()) && gravity_simulation_comp_)
		r_body->set_compute_shader(gravity_simulation_comp_->get_id());

 entities_.emplace(node_id, r_body);
	order_id_.push_back(node_id);
	order_index_[node_id] = order_id_.size() - 1;
	previous_positions_[node_id] = r_body->get_position();
	current_positions_[node_id] = r_body->get_position();
	compute_groups_[r_body->get_compute_shader_id()].push_back(r_body);
	gpu_buffer_dirty_ = true;
	

	return true;
}

void physics_system::compute_cpu() {
	std::vector<std::pair<uuid, rigid_body*>> all(entities_.begin(), entities_.end());

	for (size_t i = 0; i < all.size(); ++i)
	{
		auto [a_node, a_body] = all[i];
		for (size_t j = i + 1; j < all.size(); ++j)
		{
			auto [b_node, b_body] = all[j];

			glm::vec3 dir = b_body->get_position() - a_body->get_position();
			float dist = glm::length(dir);

			if (dist < 1e-3f) continue;

			glm::vec3 force_dir = glm::normalize(dir);
			float force_mag = u_sys_->scaled_G() * a_body->get_mass() * b_body->get_mass() / (dist * dist);
			glm::vec3 force = force_dir * force_mag;

			a_body->apply_force(force);
			b_body->apply_force(-force);
		}
	}
}

void physics_system::compute_gpu(const float& dt) {
	constexpr GLuint local_size_x = 64;

	for (auto& [shader_id, bodies] : compute_groups_) {
		auto shader_it = compute_shaders_.find(shader_id);
		if (shader_it == compute_shaders_.end() || !shader_it->second || !shader_it->second->is_vaild() || bodies.empty())
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

		std::vector<physics_data> gpu_result;
		if (requires_cpu_readback) {
			auto section = frame_profiler::measure_active("fixed_update_gpu_readback");
			if (readback_pending_[shader_id])
				compute->try_readback<physics_data>(0, gpu_result);
		}
		else {
			readback_pending_[shader_id] = false;
		}

		if (requires_cpu_readback) {
			auto section = frame_profiler::measure_active("fixed_update_gpu_apply_result");
			const size_t count = std::min(bodies.size(), gpu_result.size());
			for (size_t i = 0; i < count; ++i) {
				auto* body = bodies[i];
				if (!body || !body->get_node() || !body->get_compute_data())
					continue;

				*body->get_compute_data() = gpu_result[i];
				current_positions_[body->get_node()->get_id()] = gpu_result[i].position;
			}
		}

		const GLuint groups_x = static_cast<GLuint>((bodies.size() + local_size_x - 1) / local_size_x);
		{
			auto section = frame_profiler::measure_active("fixed_update_gpu_dispatch");
			compute->use();
			compute->set_uni_float("G", u_sys_->scaled_G());
			compute->set_uni_float("dt", u_sys_->time(dt) * simulation_speed_);
			compute->set_uni_float("rawDt", dt);
			compute->set_uni_float("simulationTime", simulation_time_);
			compute->dispatch({groups_x, 1, 1});
           if (requires_cpu_readback) {
				compute->enqueue_readback<physics_data>(0);
				readback_pending_[shader_id] = true;
			}
		}
	}

	gpu_buffer_dirty_ = false;
}


bool physics_system::remove(rigid_body* rigid_body) {
	uuid node_id = rigid_body->get_node()->get_id();

	if (entities_.contains(node_id))
	{
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
		gpu_buffer_dirty_ = true;


		return true;
	}

	return false;
}

void physics_system::register_in(compute_shader* c_shader) {
	if (!c_shader) return;
	compute_shaders_[c_shader->get_id()] = c_shader;
}

void physics_system::set_gpu_driven_nodes(const std::unordered_set<uuid>& node_ids) {
	gpu_driven_nodes_ = node_ids;
}

void physics_system::sync_scene_positions(float alpha) const {
    auto section = frame_profiler::measure_active("scene_sync_render_apply_positions");
	for (const auto& id : order_id_)
	{
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
	simulation_time_ += dt;

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
		for (auto& entity : entities_ | std::views::values)
		{
			const auto id = entity->get_node()->get_id();
			entity->integrate(dt);
			current_positions_[id] = entity->get_position();
		}
	}

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
