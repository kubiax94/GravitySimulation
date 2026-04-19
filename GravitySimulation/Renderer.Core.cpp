#include "Renderer.h"

#include <cstdint>

namespace {
	uint64_t hash_visual_scale(const glm::vec3& scale) {
		return static_cast<uint64_t>(std::hash<float>{}(scale.x))
			^ (static_cast<uint64_t>(std::hash<float>{}(scale.y)) << 1)
			^ (static_cast<uint64_t>(std::hash<float>{}(scale.z)) << 2);
	}

	glm::mat4 apply_visual_scale(const glm::mat4& model, const glm::vec3& visual_scale) {
		glm::mat4 scaled = model;
		const glm::vec4 scale(visual_scale, 1.0f);
		for (int column = 0; column < 4; ++column)
			scaled[column] *= scale;

		return scaled;
	}
}

void renderer::initialize() {
}

void renderer::set_visual_scale(const glm::vec3& scalar) {
	visual_scale_ = scalar;
}

type_id_t renderer::get_type_id() const
{
	return ::get_type_id<renderer>();
}

renderer::renderer(scene_node* s_node, shader* shader, Mesh* mesh) : transformable(s_node, s_node) {
	this->shader_ = shader;
	this->mesh_ = mesh;

	initialize();	
}

renderer::renderer(scene_node* s_node, shader* shader, Mesh* mesh, const float& v_scale) : renderer(s_node, shader, mesh){
	this->visual_scale_ = glm::vec3(v_scale);
}

void renderer::attach_to(scene_node* n_node) {
	transformable::attach_to(n_node);
	if (auto* s_manager = n_node->get_scene_manager())
		s_manager->register_in(this);
}

bool renderer::detach() {
	if (auto* node = get_node()) {
		if (auto* s_manager = node->get_scene_manager())
			s_manager->register_out(this);
	}
	return transformable::detach();
}

glm::mat4 renderer::get_visual_model_matrix() const {
	return apply_visual_scale(get_transform()->get_global_matrix_model(), visual_scale_);
}

glm::mat4 renderer::get_visual_model_matrix_without_translation() const {
	glm::mat4 model = apply_visual_scale(get_transform()->get_global_matrix_model(), visual_scale_);
	model[3] = glm::vec4(0.f, 0.f, 0.f, 1.f);
	return model;
}

uint64_t renderer::get_instance_revision(bool ignore_translation) const {
	const auto scale_hash = hash_visual_scale(visual_scale_);

	if (!ignore_translation) {
		const auto* node = get_node();
		const uint64_t transform_revision = node ? node->get_transform_revision() : 0;

		return transform_revision ^ (scale_hash << 1);
	}

	const auto* node = get_node();
	const uint64_t transform_revision = node ? node->get_orientation_revision() : 0;
	return transform_revision ^ (scale_hash << 1);
}
