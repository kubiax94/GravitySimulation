#include "instance_manager.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_inverse.hpp>

void instance_manager::draw_instanced(const std::vector<renderer*>& renderers,
	Camera* camera,
	const std::function<void(shader&)>& pre_draw) const {
	if (!camera || renderers.empty())
		return;

	std::unordered_map<batch_key, std::vector<const renderer*>, batch_key_hash> batches;
	for (auto* render : renderers) {
		if (!render || !render->get_shader() || !render->get_mesh())
			continue;

		batches[{render->get_shader(), render->get_mesh()}].push_back(render);
	}

	int fbw = 1280, fbh = 720;
	GLFWwindow* ctx = glfwGetCurrentContext();
	if (ctx) glfwGetFramebufferSize(ctx, &fbw, &fbh);
	const float aspect = (fbh == 0) ? 1.f : static_cast<float>(fbw) / static_cast<float>(fbh);
	const glm::mat4 projection = camera->GetProjectionMatrix(aspect);
	const glm::mat4 view = camera->GetViewMatrix();

	for (auto& [key, batch] : batches) {
		if (batch.empty())
			continue;

		std::vector<glm::mat4> models;
		models.reserve(batch.size());
		for (const auto* render : batch)
			models.push_back(render->get_visual_model_matrix());

		auto* batch_shader = key.shader_ptr;
		auto* batch_mesh = key.mesh_ptr;
		batch_shader->use();
		batch_shader->set_uni_int("useInstancing", 1);
		batch_shader->set_uniform_mat4("view", view);
		batch_shader->set_uniform_mat4("projection", projection);
		batch_shader->set_uniform_mat4("model", glm::mat4(1.0f));

		try {
			glm::mat4 invView = glm::inverse(view);
			glm::vec3 camPos = glm::vec3(invView[3]);
			batch_shader->set_uni_vec3("viewPos", camPos);
			batch_shader->set_uni_vec3("lightPos", camPos + glm::vec3(0.0f, 0.0f, 10.0f));
			batch_shader->set_uni_vec3("lightColor", glm::vec3(1.0f, 0.85f, 0.6f));
			batch_shader->set_uni_vec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));
		}
		catch (...) {}

		if (pre_draw) pre_draw(*batch_shader);

		batch_mesh->UpdateInstanceModels(models);
		batch_mesh->DrawInstanced(static_cast<GLsizei>(models.size()));
	}
}
