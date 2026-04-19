#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"
#include <iostream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>

namespace {
render_frame_context build_frame_context(Camera* camera) {
	render_frame_context frame_context;
	if (!camera)
		return frame_context;

	int fbw = 1280;
	int fbh = 720;
	GLFWwindow* ctx = glfwGetCurrentContext();
	if (ctx)
		glfwGetFramebufferSize(ctx, &fbw, &fbh);

	const float aspect = (fbh == 0) ? 1.f : static_cast<float>(fbw) / static_cast<float>(fbh);
	frame_context.projection = camera->GetProjectionMatrix(aspect);
	frame_context.view = camera->GetViewMatrix();

	try {
		glm::mat4 invView = glm::inverse(frame_context.view);
		frame_context.camera_position = glm::vec3(invView[3]);
	}
	catch (...) {}

	return frame_context;
}
}

void renderer::draw(Camera* c, const std::function<void(shader&)>& pre_draw) const
{
	draw(build_frame_context(c), pre_draw);
}

void renderer::draw(const render_frame_context& frame_context, const std::function<void(shader&)>& pre_draw) const
{

	shader_->use();
	shader_->set_uni_int("useInstancing", 0);
	shader_->set_uni_int("useGpuPositions", 0);

    shader_->set_uniform_mat4("view", frame_context.view);
	shader_->set_uniform_mat4("projection", frame_context.projection);
	shader_->set_uniform_mat4("model", get_visual_model_matrix());

	try {
     shader_->set_uni_vec3("viewPos", frame_context.camera_position);
		shader_->set_uni_vec3("lightPos", frame_context.camera_position + glm::vec3(0.0f, 0.0f, 10.0f));
		shader_->set_uni_vec3("lightColor", glm::vec3(1.0f, 0.85f, 0.6f));
		shader_->set_uni_vec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
	}
	catch(...) {}
	if (pre_draw) pre_draw(*shader_);

	mesh_->Draw();

}
