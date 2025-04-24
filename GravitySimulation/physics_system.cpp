#include "physics_system.h"

const float G = 1.f;
//float scale_mass = 1e24f; // masa Ziemi
//float scale_distance = 1e6f; // 1mln km
//float scale_time = 3.872e6f / 3600.f; // 1h

bool physics_system::add(rigid_body* r_body) {
	auto* test = r_body->get_node();
	entities_[test] = r_body;

	return true;
}

void physics_system::gravity_simulation() {
	std::vector<std::pair<scene_node*, rigid_body*>> all(entities_.begin(), entities_.end());

	for (size_t i = 0; i < all.size(); ++i)
	{
		auto [a_node, a_body] = all[i];
		for (size_t j = i + 1; j < all.size(); ++j)
		{
			auto [b_node, b_body] = all[j];

			glm::vec3 dir = b_node->get_global_position() - a_node->get_global_position();
			float dist = glm::length(dir);

			if (dist < 1e-3f) continue;

			glm::vec3 force_dir = glm::normalize(dir);
			float force_mag = G * a_body->mass * b_body->mass / (dist * dist);
			glm::vec3 force = force_dir * force_mag;

			a_body->apply_force(force);
			b_body->apply_force(-force);
		}
	}
}

bool physics_system::remove(rigid_body* rigid_body) {
	if (entities_.contains(rigid_body->get_node())) {
		entities_.erase(rigid_body->get_node());

		return true;
	}

	return false;
}

void physics_system::update(const float& dt) {

	gravity_simulation();

	for (auto& entity : entities_ | std::views::values)
		entity->integrate(dt);

}
