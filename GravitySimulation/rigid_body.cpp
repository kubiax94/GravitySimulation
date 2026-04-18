#include "rigid_body.h"

#include <glm/gtx/string_cast.hpp>

type_id_t rigid_body::type_id() {
	return ::get_type_id<rigid_body>();
}

rigid_body::rigid_body(scene_node* owner, physics_data& p_data) : component(owner), p_data_(&p_data) {
}

rigid_body::rigid_body(scene_node* owner, physics_data* p_data) : component(owner), p_data_(p_data) {
}

void rigid_body::set_mass(const float& n_mass) const {
	p_data_->position.w = n_mass;
	p_data_->velocity.w = n_mass;
	p_data_->accumulated_force.w = n_mass;
}

glm::vec3 rigid_body::get_position() const {
	return p_data_->position;
}

float rigid_body::get_mass() const {
	return p_data_->position.w;
}

glm::vec3 rigid_body::get_velocity() const {
	return glm::vec3(p_data_->velocity);
}

type_id_t rigid_body::get_type_id() const {
	return type_id();
}

physics_data* rigid_body::get_compute_data() {
	return p_data_;
}

uuid rigid_body::get_compute_shader_id() const {
    return compute_shader_id_;
}

void rigid_body::set_compute_shader(const uuid& c_shader_uuid) {
    compute_shader_id_ = c_shader_uuid;
}

void rigid_body::attach_to(scene_node* n_node) {
	component::attach_to(n_node);
	n_node->set_global_position(p_data_->position);

	if (auto* s_manager = owner_node_->get_scene_manager())
		s_manager->register_in(this);
}

void rigid_body::apply_force(const glm::vec3& force) {
	p_data_->accumulated_force += glm::vec4(force, 0.f);
}

void rigid_body::clear_force() {
	p_data_->accumulated_force = glm::vec4(glm::vec3(0.f), get_mass());
}

void rigid_body::apply_to_scene() const {
	if (get_node())
		get_node()->set_global_position(this->p_data_->position);
}

void rigid_body::set_position(const glm::vec3& n_pos) {
	float mass = get_mass();
	p_data_->position = glm::vec4(n_pos, mass);
}

bool rigid_body::detach() {
	if (auto* s_manager = owner_node_->get_scene_manager())
		s_manager->register_out(this);

	bool d_cond = component::detach();
	return d_cond;
}

void rigid_body::integrate(const float& dt) {
	if (get_mass() <= 0.f) return;

	glm::vec3 acceleration = p_data_->accumulated_force / get_mass();
	p_data_->velocity += glm::vec4(acceleration * dt, 0.f);

	p_data_->position += glm::vec4(glm::vec3(p_data_->velocity * dt), 0.f);

	if (get_node())
		get_node()->set_global_position(p_data_->position);
	clear_force();

}

rigid_body::~rigid_body() {
}

void* rigid_body::get_buffer_data() const {
	return p_data_;
}

size_t rigid_body::get_buffer_size() const {
    return sizeof(physics_data);
}

size_t rigid_body::get_buffer_count() const {
    return 1;
}
