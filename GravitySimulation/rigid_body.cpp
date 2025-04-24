#include "rigid_body.h"

#include <glm/gtx/string_cast.hpp>

type_id_t rigid_body::type_id() {
	return ::get_type_id<rigid_body>();
}

rigid_body::rigid_body(scene_node* owner, const physics_data& p_data) : component(owner), physics_data(p_data) {}

type_id_t rigid_body::get_type_id() const {
	return type_id();
}

void rigid_body::attach_to(scene_node* n_node) {
	component::attach_to(n_node);

	if (auto* s_manager = owner_node_->get_scene_manager())
		s_manager->register_in(this);
	
}

void rigid_body::apply_force(const glm::vec3& force) {
	accumulated_force += force;
}

void rigid_body::clear_force() {
	accumulated_force = glm::vec3(0.f);
}

bool rigid_body::detach() {
	bool d_cond = component::detach();
	if (auto* s_manager = owner_node_->get_scene_manager())
		s_manager->register_in(this);

	return d_cond;
}

void rigid_body::integrate(const float& dt) {
	if (mass <= 0.f) return;

	glm::vec3 acceleration = accumulated_force / mass;
	velocity += acceleration * dt;

	auto* node = get_node();

	glm::vec3 pos = node->get_global_position();
	pos += velocity * dt;

	node->set_global_position(pos);

	clear_force();

}

rigid_body::~rigid_body() {
}
