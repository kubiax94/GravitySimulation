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

bounding_box build_mesh_local_bounding_box(const Mesh* mesh, const glm::vec3& visual_scale) {
	bounding_box bounds;
	if (!mesh)
		return bounds;

	const auto& mesh_data = mesh->get_mesh_data();
	if (!mesh_data)
		return bounds;

	for (const auto& vertex : mesh_data->vertecies)
		bounds.encapsulate(vertex.Position * visual_scale);

	return bounds;
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
 shader_->set_uni_int("useGpuPositions", frame_context.use_gpu_positions ? 1 : 0);
	shader_->set_uni_int("instanceBaseIndex", 0);
	shader_->set_uni_int("physicsBodyIndex", frame_context.physics_body_index);
	if (frame_context.use_gpu_positions && frame_context.physics_ssbo != 0)
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, frame_context.physics_ssbo);

    shader_->set_uniform_mat4("view", frame_context.view);
	shader_->set_uniform_mat4("projection", frame_context.projection);
	shader_->set_uniform_mat4("model", get_visual_model_matrix());

	try {
     shader_->set_uni_vec3("viewPos", frame_context.camera_position);
		shader_->set_uni_vec3("lightPos", frame_context.camera_position + glm::vec3(0.0f, 0.0f, 10.0f));
		shader_->set_uni_vec3("lightColor", glm::vec3(1.0f, 0.85f, 0.6f));
		shader_->set_uni_vec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
       shader_->set_uni_float("time", static_cast<float>(glfwGetTime()));
	}
	catch(...) {}
   apply_material(*shader_);
	if (pre_draw) pre_draw(*shader_);

	mesh_->Draw();
	if (frame_context.use_gpu_positions && frame_context.physics_ssbo != 0)
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

}

bounding_box renderer::get_local_bounding_box() const {
    if (!local_bounding_box_cached_) {
		local_bounding_box_cache_ = build_mesh_local_bounding_box(mesh_, visual_scale_ * renderer_scale);
		local_bounding_box_cached_ = true;
	}

	return local_bounding_box_cache_;
}

bounding_box renderer::get_world_bounding_box() const {
 const uint64_t revision = get_instance_revision(false);
	if (world_bounding_box_revision_ != revision) {
		world_bounding_box_cache_ = transform_bounding_box(get_local_bounding_box(), get_visual_model_matrix());
		world_bounding_box_revision_ = revision;
	}

	return world_bounding_box_cache_;
}
