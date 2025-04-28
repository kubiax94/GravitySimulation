
#include "Renderer.h"
#include <iostream>


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

glm::mat4 renderer::get_visual_model_matrix() const {
	glm::mat4 model = get_transform()->get_global_matrix_model();
	return glm::scale(glm::mat4(1.0f), visual_scale_) * model;
}

void renderer::draw(Camera* c, const std::function<void(shader&)>& pre_draw) const
{

	auto projection = c->GetProjectionMatrix(1280 / 720);
	auto view = c->GetViewMatrix();

	shader_->use();

	shader_->set_uniform_mat4("view", view);
	shader_->set_uniform_mat4("projection", projection);
	shader_->set_uniform_mat4("model", get_visual_model_matrix());
	if (pre_draw) pre_draw(*shader_);

	mesh_->Draw();

}
