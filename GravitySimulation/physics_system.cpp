#include "physics_system.h"
#include <glm/gtx/string_cast.hpp>

//float scale_mass = 1e24f; // masa Ziemi
//float scale_distance = 1e6f; // 1mln km
//float scale_time = 3.872e6f / 3600.f; // 1h

std::vector<physics_data> physics_system::get_physics_data() const {
    // avoid reallocating vector each frame: caller should reuse buffer when possible
    std::vector<physics_data> data;
    data.reserve(order_id_.size());

    for (size_t i = 0; i < order_id_.size(); ++i)
        data.push_back(*entities_.at(order_id_[i])->get_compute_data());

    return data;
}

//FOR NOW IT'S CREATING BY DEFAULT COMPUTE SHADER WITH SSBO ATTACHED TO PHYCIS_DATA
physics_system::physics_system(unit_system* u_sys): u_sys_(u_sys) {
	auto* gravity_comp = new compute_shader("gravity_simulation.glsl");
	// store pointer to the default gravity compute shader so later calls use a valid object
	gravity_simulation_comp_ = gravity_comp;
	register_in(gravity_comp);
	gravity_comp->add_ssbo(0, get_physics_data());
}

bool physics_system::add(rigid_body* r_body) {
	auto* node = r_body->get_node();

	if (entities_.contains(node->get_id())) return false;
	entities_[node->get_id()] = r_body;
	order_id_.push_back(node->get_id());
	previous_positions_[node->get_id()] = r_body->get_position();
	current_positions_[node->get_id()] = r_body->get_position();
	compute_groups_[r_body->get_compute_shader_id()].push_back(r_body);
	

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

void physics_system::compute_gpu() {
    // reuse local buffer to avoid allocating every frame
    static std::vector<physics_data> gpu_data;
    gpu_data.clear();
    gpu_data.reserve(order_id_.size());
    for (size_t i = 0; i < order_id_.size(); ++i)
        gpu_data.push_back(*entities_.at(order_id_[i])->get_compute_data());

    gravity_simulation_comp_->update_ssbo(0, gpu_data);

    constexpr GLuint local_size_x = 64;
    const GLuint groups_x = static_cast<GLuint>((gpu_data.size() + local_size_x - 1) / local_size_x);
    gravity_simulation_comp_->dispatch({groups_x, 1, 1});

    static std::vector<physics_data> gpu_result;
    gravity_simulation_comp_->get_binding_data(0, gpu_result);

    if (!gpu_result.empty()) {
        for (size_t i = 0; i < order_id_.size() && i < gpu_result.size(); ++i)
        {
            const auto& id = order_id_[i];
            auto* body = entities_[id];
            auto* p_data = body->get_compute_data();
            if (p_data) {
                previous_positions_[id] = current_positions_[id];
                current_positions_[id] = glm::vec3(gpu_result[i].position);
                *p_data = gpu_result[i];
            }
        }
    }
}


bool physics_system::remove(rigid_body* rigid_body) {
	uuid node_id = rigid_body->get_node()->get_id();

	if (entities_.contains(node_id))
	{
		entities_.erase(node_id);
		previous_positions_.erase(node_id);
		current_positions_.erase(node_id);


		return true;
	}

	return false;
}

void physics_system::register_in(compute_shader* c_shader) {
	if (!c_shader) return;
	compute_shaders_[c_shader->get_id()] = c_shader;
}

void physics_system::sync_scene_positions(float alpha) const {
	const bool gpu_mode = gravity_simulation_comp_ && gravity_simulation_comp_->is_vaild();

	for (const auto& id : order_id_)
	{
		auto entityIt = entities_.find(id);
		auto prevIt = previous_positions_.find(id);
		auto currIt = current_positions_.find(id);
		if (entityIt == entities_.end() || prevIt == previous_positions_.end() || currIt == current_positions_.end())
			continue;

		auto* node = entityIt->second->get_node();
		if (!node)
			continue;

		const glm::vec3 position = gpu_mode
			? currIt->second
			: glm::mix(prevIt->second, currIt->second, alpha);

		node->set_global_position(position);
	}
}

void physics_system::update(const float& dt) {


    //compute_cpu();
    // Ensure we have a valid compute shader, bind it, then set uniforms and dispatch
    if (gravity_simulation_comp_ && gravity_simulation_comp_->is_vaild()) {
        gravity_simulation_comp_->use();
        gravity_simulation_comp_->set_uni_float("G", u_sys_->scaled_G());
        // pass delta time scaled exactly once
        gravity_simulation_comp_->set_uni_float("dt", u_sys_->time(dt) * simulation_speed_);
        // dispatch compute, GPU will integrate positions
        compute_gpu();
    }
    else {
        std::cerr << "physics_system::update: gravity_simulation_comp_ is null or not loaded\n";
    }

	if (!gravity_simulation_comp_ || !gravity_simulation_comp_->is_vaild()) {
		for (auto& entity : entities_ | std::views::values)
		{
			const auto id = entity->get_node()->get_id();
			previous_positions_[id] = current_positions_[id];
			entity->integrate(dt);
			current_positions_[id] = entity->get_position();
		}
	}

	// advance frame counter for readback scheduling
	++frame_idx_;
}
