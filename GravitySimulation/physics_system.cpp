#include "physics_system.h"

#include <glm/gtx/string_cast.hpp>

//float scale_mass = 1e24f; // masa Ziemi
//float scale_distance = 1e6f; // 1mln km
//float scale_time = 3.872e6f / 3600.f; // 1h

std::vector<physics_data> physics_system::get_physics_data() const {

	std::vector<physics_data> data(order_id_.size());

	for (auto i = 0; i < order_id_.size(); i++)
		data[i] = static_cast<physics_data>(*entities_.at(order_id_[i]));

	return  data;
}

physics_system::physics_system(unit_system* u_sys): u_sys_(u_sys) {
	gravity_simulation_comp_ = new compute_shader("gravity_simulation.glsl");
}

bool physics_system::add(rigid_body* r_body) {
	
	auto* node = r_body->get_node();

	if (entities_.contains(node->get_id())) return false;
	entities_[node->get_id()] = r_body;
	order_id_.push_back(node->get_id());

	return true;
}

void physics_system::gravity_simulation_cpu() {
	std::vector<std::pair<uuid, rigid_body*>> all(entities_.begin(), entities_.end());

	for (size_t i = 0; i < all.size(); ++i)
	{
		auto [a_node, a_body] = all[i];
		for (size_t j = i + 1; j < all.size(); ++j)
		{
			auto [b_node, b_body] = all[j];

			glm::vec3 dir = b_body->position - a_body->position;
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

void physics_system::gravity_simulation_gpu() {

	auto gpu_data = get_physics_data();

	gravity_simulation_comp_->update_ssbo(gpu_data);
	auto gpu_result = gravity_simulation_comp_->compute(entities_.size());

	for (auto i = 0; i < order_id_.size(); i++) {
		entities_[order_id_[i]]->position = gpu_result[i].position;
		entities_[order_id_[i]]->velocity = gpu_result[i].velocity;
		entities_[order_id_[i]]->accumulated_force = gpu_result[i].accumulated_force;

	}
}


bool physics_system::remove(rigid_body* rigid_body) {

	uuid node_id = rigid_body->get_node()->get_id();

	if (entities_.contains(node_id)) {
		entities_.erase(node_id);
		

		return true;
	}

	return false;
}

void physics_system::update(const float& dt) {

	//gravity_simulation_cpu();
	gravity_simulation_comp_->set_uni_float("G", u_sys_->scaled_G());
	gravity_simulation_gpu();

	glm::vec3 total_momentum = glm::vec3(0);
	for (auto& entity : entities_ | std::views::values) {
		entity->integrate(dt);
		total_momentum += entity->get_mass() * entity->velocity;
	}

	std::cout << glm::to_string(total_momentum) << std::endl;
}
