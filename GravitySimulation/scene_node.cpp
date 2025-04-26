#include "scene_node.h"

void scene_node::set_dirty() {

	//Dla uniknięcia przy masowych zmianach, trzeba to sprzwidzć.
	if (dirty_transform_)
		return;

	dirty_transform_ = true;

	for (const auto& child : children_ | std::views::values)
		child->set_dirty();
}

scene_node::scene_node(const std::string& name, scene_node* parent) {
	this->name_ = name;
	this->parent_ = parent;

	if (parent != nullptr)
		parent->add_child(this);
}

scene_node::scene_node(const std::string& name, scene_node* parent, i_scene_manager* scene_manager)
	: name_(name), parent_(parent), scene_manager_(scene_manager) {
}

void scene_node::add_child(scene_node* n_node) {
	n_node->parent_ = this;
	children_[n_node->name_] = n_node;
}
bool scene_node::add_component(component* comp) {
	comp->attach_to(this);
	components_[comp->get_type_id()] = comp;

	return comp;
}

i_scene_manager* scene_node::get_scene_manager() const {
	if (scene_manager_)
		return scene_manager_;

	//TODO: Ewentualne przeszukanie sceny lub proba przypisania

	return nullptr;
}

void scene_node::remove_component(component* component) {
	component->detach();
	components_.erase(component->get_type_id());
}

void scene_node::update() {
}

void scene_node::draw() {
}

const glm::vec3& scene_node::get_position() const {
	return transform_.GetPosition();
}

const glm::vec3& scene_node::get_rotation() const {
	return transform_.GetRotation();
}

const glm::vec3& scene_node::get_scale() const {
	return transform_.GetScale();
}

//Nie robić operacji na tym transform! Do działania na danych służy i_transofrmable
const transform& scene_node::get_transform() const {
	return transform_;
}

const glm::vec3& scene_node::get_global_scale() const {
	auto model = get_global_matrix_model();

	return glm::vec3(
		glm::length(glm::vec3(model[0])),
		glm::length(glm::vec3(model[1])),
		glm::length(glm::vec3(model[2]))
	);
}

const glm::mat4& scene_node::get_global_matrix_model() const {
	//Chce tylko do głównego obiektu to powinno zakonczyń na root.
	if (dirty_transform_) {

		if (parent_)
			global_matrix_model_ = parent_->get_global_matrix_model() * transform_.get_local_model_matrix();
		else
			global_matrix_model_ = transform_.get_local_model_matrix();

		dirty_transform_ = false;
	}

	return global_matrix_model_;
}

void scene_node::set_global_position(const glm::vec3& n_pos) {
	if (parent_)
	{
		glm::mat4 inv = glm::inverse(parent_->get_global_matrix_model());
		glm::vec4 local = inv * glm::vec4(n_pos, 1.0f);
		set_position(glm::vec3(local));
	} else
		set_position(n_pos);
	
}

void scene_node::set_position(const glm::vec3& n_pos) {
	transform_.setPosition(n_pos);
	set_dirty();
}

void scene_node::set_position(const float& x, const float& y, const float& z) {
	set_position(glm::vec3(x, y, z));
}

void scene_node::set_global_rotation(const glm::vec3& global_euler_deg) {
	if (parent_)
	{
		glm::quat global_quat = glm::quat(glm::radians(global_euler_deg));
		glm::quat parent_global_quat = glm::quat(glm::radians(parent_->get_global_rotation()));
		glm::quat local_quat = glm::inverse(parent_global_quat) * global_quat;

		transform_.setRotation(glm::degrees(glm::eulerAngles(local_quat)));

	} else
	{
		transform_.setRotation(global_euler_deg);
	}

	set_dirty();
}

void scene_node::set_rotation(const glm::vec3& n_rot) {
	transform_.setRotation(n_rot);
	set_dirty();
}

void scene_node::set_rotation(const float& x, const float& y, const float& z) {
	set_rotation(glm::vec3(x, y, z));
}

void scene_node::set_scale(const float& x) {
	set_scale(glm::vec3(x));
}

void scene_node::set_scale(const glm::vec3& n_sc) {
	transform_.setScale(n_sc);
	set_dirty();
}

void scene_node::set_scale(const float& x, const float& y, const float& z) {
	set_scale(glm::vec3(x, y, z));
}

void scene_node::set_transform(const transform& new_transform) {
	this->transform_ = new_transform;
	set_dirty();
}

const glm::vec3& scene_node::forward() const {
	return -glm::normalize(get_global_matrix_model()[2]);
}

const glm::vec3& scene_node::forward_local() const {
	return transform_.forward();
}

const glm::vec3& scene_node::right() const {
	return glm::normalize(get_global_matrix_model()[0]);
}

const glm::vec3& scene_node::right_local() const {
	return transform_.right();
}

const glm::vec3& scene_node::up() const {
	return glm::normalize(get_global_matrix_model()[1]);
}

const glm::vec3& scene_node::up_local() const {
	return transform_.up();
}

glm::vec3 scene_node::get_global_position() const {
	return glm::vec3(get_global_matrix_model()[3]);
}

glm::vec3 scene_node::get_global_rotation() {
	auto model = get_global_matrix_model();
	const auto scale = get_global_scale();

	model[0] /= scale.x;
	model[1] /= scale.y;
	model[2] /= scale.z;

	return glm::degrees(glm::eulerAngles(glm::quat_cast(model)));
}