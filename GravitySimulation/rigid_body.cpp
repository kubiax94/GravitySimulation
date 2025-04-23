#include "rigid_body.h"

type_id_t rigid_body::type_id() {
	return ::get_type_id<rigid_body>();
}

rigid_body::rigid_body(scene_node* owner, const physics_data& p_data) : component(owner), physics_data(p_data) {
	
}

type_id_t rigid_body::get_type_id() const {
	return type_id();
}

void rigid_body::attach_to(scene_node* n_node) {
	component::attach_to(n_node);

	if (auto* s_manager = owner_node_->get_scene_manager())
		s_manager->register_in(this);
	
}

bool rigid_body::detach() {
	bool d_cond = component::detach();
	if (auto* s_manager = owner_node_->get_scene_manager())
		s_manager->register_in(this);

	return d_cond;
}

void rigid_body::integrate(const float& dt) {
	auto* node = get_node();
	glm::vec3 pos = get_node()->get_global_position();
	velocity += accumulated_force * dt;

	pos += velocity * dt;
	get_node()->set_position(pos);

}

rigid_body::~rigid_body() {
}
