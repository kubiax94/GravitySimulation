#include "physics_system.h"

bool physics_system::add(rigid_body* r_body) {
	auto* test = r_body->get_node();
	entities_[test] = r_body;

	return true;
}

bool physics_system::remove(rigid_body* rigid_body) {
	if (entities_.contains(rigid_body->get_node())) {
		entities_.erase(rigid_body->get_node());

		return true;
	}

	return false;
}

void physics_system::update(const float& dt) {

	for (auto& entity : entities_ | std::views::values)
		entity->integrate(dt);

}