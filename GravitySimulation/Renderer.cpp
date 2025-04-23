
#include "Renderer.h"
#include <iostream>


void renderer::initialize() {

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

void renderer::draw(Camera* c, const std::function<void(shader&)>& pre_draw) const
{

	auto projection = c->GetProjectionMatrix(1280 / 720);
	auto view = c->GetViewMatrix();

	shader_->use();

	shader_->set_uniform_mat4("view", view);
	shader_->set_uniform_mat4("projection", projection);
	shader_->set_uniform_mat4("model", owner_node_->get_global_matrix_model());

	if (pre_draw) pre_draw(*shader_);

	mesh_->Draw();

}
