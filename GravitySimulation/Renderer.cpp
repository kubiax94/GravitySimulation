#include "Renderer.h"
#include <iostream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>


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
	glm::mat4 model = get_transform()->get_global_matrix_model();
	return glm::scale(glm::mat4(1.0f), visual_scale_) * model;
}

void renderer::draw(Camera* c, const std::function<void(shader&)>& pre_draw) const
{

    // compute actual framebuffer aspect ratio to avoid integer division / mismatched projection
    int fbw = 1280, fbh = 720;
    GLFWwindow* ctx = glfwGetCurrentContext();
    if (ctx) glfwGetFramebufferSize(ctx, &fbw, &fbh);
    float aspect = (fbh == 0) ? 1.f : static_cast<float>(fbw) / static_cast<float>(fbh);
    auto projection = c->GetProjectionMatrix(aspect);
	auto view = c->GetViewMatrix();

	shader_->use();
	shader_->set_uni_int("useInstancing", 0);

	shader_->set_uniform_mat4("view", view);
	shader_->set_uniform_mat4("projection", projection);
	shader_->set_uniform_mat4("model", get_visual_model_matrix());

	try {
		glm::mat4 invView = glm::inverse(view);
		glm::vec3 camPos = glm::vec3(invView[3]);
		shader_->set_uni_vec3("viewPos", camPos);
		shader_->set_uni_vec3("lightPos", camPos + glm::vec3(0.0f, 0.0f, 10.0f));
		shader_->set_uni_vec3("lightColor", glm::vec3(1.0f, 0.85f, 0.6f));
		shader_->set_uni_vec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
	}
	catch(...) {}
	if (pre_draw) pre_draw(*shader_);

	mesh_->Draw();

}
