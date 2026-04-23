#include "scene_node.h"

#include "Renderer.h"

void scene_node::set_dirty(bool affect_non_translation) {
	dirty_transform_ = true;
	++transform_revision_;

 if (affect_non_translation)
		++orientation_revision_;

	for (const auto& child : children_ | std::views::values)
		child->set_dirty(affect_non_translation);
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
 if (!n_node)
		return;

	n_node->set_parent(this);
}

void scene_node::set_parent(scene_node* new_parent, bool keep_global_transform) {
	if (parent_ == new_parent)
		return;

	const glm::vec3 global_position = keep_global_transform ? get_global_position() : glm::vec3(0.f);
	const glm::vec3 global_rotation = keep_global_transform ? get_global_rotation() : glm::vec3(0.f);
	const glm::vec3 global_scale = keep_global_transform ? get_global_scale() : glm::vec3(1.f);

	if (parent_)
		parent_->children_.erase(name_);

	parent_ = new_parent;
	if (parent_)
		parent_->children_[name_] = this;

	if (keep_global_transform) {
		set_global_position(global_position);
		set_global_rotation(global_rotation);
		set_global_scale(global_scale);
	}
	else {
		set_dirty();
	}
}

scene_node* scene_node::get_parent() const {
	return parent_;
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

void scene_node::set_collision_layer(collision_layer layer) {
	collision_layer_mask_ = to_collision_mask(layer);
}

void scene_node::set_collision_layer_mask(collision_mask_t layer_mask) {
	collision_layer_mask_ = layer_mask;
}

collision_mask_t scene_node::get_collision_layer_mask() const {
	return collision_layer_mask_;
}

void scene_node::set_collision_query_mask(collision_mask_t query_mask) {
	collision_query_mask_ = query_mask;
}

collision_mask_t scene_node::get_collision_query_mask() const {
	return collision_query_mask_;
}

uuid scene_node::get_id() const {
	return id_;
}

uint64_t scene_node::get_transform_revision() const {
	return transform_revision_;
}

uint64_t scene_node::get_orientation_revision() const {
	return orientation_revision_;
}

void scene_node::update() {
   for (auto* comp : components_ | std::views::values) {
		if (comp)
			comp->update();
	}

	for (auto* child : children_ | std::views::values) {
		if (child)
			child->update();
	}
}

void scene_node::draw() {
}

bounding_box scene_node::get_subtree_world_bounding_box() const {
	bounding_box bounds;

   const auto renderer_it = components_.find(get_type_id<renderer>());
	if (renderer_it != components_.end()) {
		if (auto* node_renderer = dynamic_cast<renderer*>(renderer_it->second))
			bounds.encapsulate(node_renderer->get_world_bounding_box());
	}

	for (const auto& child : children_ | std::views::values) {
		if (child)
			bounds.encapsulate(child->get_subtree_world_bounding_box());
	}

	return bounds;
}

bounding_box scene_node::get_local_subtree_bounding_box() const {
	bounding_box world_bounds = get_subtree_world_bounding_box();
	if (!world_bounds.valid)
		return world_bounds;

	return transform_bounding_box(world_bounds, glm::inverse(get_global_matrix_model()));
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

glm::vec3 scene_node::get_global_scale() const {
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
    set_dirty(false);
}

void scene_node::set_position(const float& x, const float& y, const float& z) {
	set_position(glm::vec3(x, y, z));
}

void scene_node::set_global_rotation(const glm::vec3& global_euler_deg) {
	if (parent_)
	{
		glm::quat global_quat = glm::quat(glm::radians(global_euler_deg));

		glm::quat parent_global_quat = glm::quat_cast(parent_->get_transform().get_local_model_matrix());
;		glm::quat local_quat = glm::inverse(parent_global_quat) * global_quat;

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

void scene_node::set_global_scale(const glm::vec3& scalar) {
	if (parent_)
	{
		glm::vec3 local_scale = scalar / parent_->get_global_scale();
		set_scale(local_scale);
	}
	else
	{
		set_scale(scalar);
	}
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

glm::vec3 scene_node::forward() const {
	return -glm::normalize(glm::vec3(get_global_matrix_model()[2]));
}

const glm::vec3& scene_node::forward_local() const {
	return transform_.forward();
}

glm::vec3 scene_node::right() const {
	return glm::normalize(glm::vec3(get_global_matrix_model()[0]));
}

const glm::vec3& scene_node::right_local() const {
	return transform_.right();
}

glm::vec3 scene_node::up() const {
	return glm::normalize(glm::vec3(get_global_matrix_model()[1]));
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