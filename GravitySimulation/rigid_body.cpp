#include "rigid_body.h"

#include <glm/gtx/string_cast.hpp>

type_id_t rigid_body::type_id() {
	return ::get_type_id<rigid_body>();
}

rigid_body::rigid_body(scene_node* owner, const physics_data& p_data) : component(owner), physics_data(p_data) {}

rigid_body::rigid_body(scene_node* owner, const physics_data* p_data) : component(owner), physics_data(*p_data) {
}

void rigid_body::set_mass(const float& n_mass) {
	position.w = n_mass;
	velocity.w = n_mass;
	accumulated_force.w = n_mass;
}

glm::vec3 rigid_body::get_position() const {
	return position;
}

float rigid_body::get_mass() const {
	return position.w;
}

type_id_t rigid_body::get_type_id() const {
	return type_id();
}

void rigid_body::attach_to(scene_node* n_node) {
	component::attach_to(n_node);
	n_node->set_global_position(position);

	if (auto* s_manager = owner_node_->get_scene_manager())
		s_manager->register_in(this);
}

void rigid_body::apply_force(const glm::vec3& force) {
	accumulated_force += glm::vec4(force, 0.f);
}

void rigid_body::clear_force() {
	accumulated_force = glm::vec4(glm::vec3(0.f), get_mass());
}

void rigid_body::apply_to_scene() const {
	if (get_node())
		get_node()->set_global_position(this->position);
}

void rigid_body::set_position(const glm::vec3& n_pos) {
	float mass = get_mass();
	position = glm::vec4(n_pos, mass);
}

bool rigid_body::detach() {
	bool d_cond = component::detach();
	if (auto* s_manager = owner_node_->get_scene_manager())
		s_manager->register_in(this);

	return d_cond;
}

void rigid_body::integrate(const float& dt) {
	if (get_mass() <= 0.f) return;

	glm::vec3 acceleration = accumulated_force / get_mass();
	velocity += glm::vec4(acceleration * dt, 0.f);

	position += glm::vec4(glm::vec3(velocity * dt), 0.f);

	if (get_node())
		get_node()->set_global_position(position);

	clear_force();

}

rigid_body::~rigid_body() {
}
