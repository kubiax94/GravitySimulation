#include "instance_manager.h"

#include "Shader.h"
#include "Mesh.h"

void instance_manager::draw_instanced(shader* batch_shader,
	Mesh* batch_mesh,
	const std::vector<renderer*>& renderers,
  const std::vector<glm::mat4>& instance_models,
	const render_frame_context& frame_context,
	GLuint physics_ssbo,
	int instance_base_index,
	bool use_gpu_positions,
	const std::function<void(shader&)>& pre_draw) const {
	if (!batch_shader || !batch_mesh || renderers.empty())
		return;

	batch_shader->use();
	batch_shader->set_uni_int("useInstancing", 1);
	batch_shader->set_uni_int("useGpuPositions", use_gpu_positions ? 1 : 0);
	batch_shader->set_uni_int("instanceBaseIndex", instance_base_index);
	batch_shader->set_uniform_mat4("view", frame_context.view);
	batch_shader->set_uniform_mat4("projection", frame_context.projection);
	batch_shader->set_uniform_mat4("model", glm::mat4(1.0f));
	if (use_gpu_positions && physics_ssbo != 0)
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, physics_ssbo);

	batch_shader->set_uni_vec3("viewPos", frame_context.camera_position);
	batch_shader->set_uni_vec3("lightPos", frame_context.camera_position + glm::vec3(0.0f, 0.0f, 10.0f));
	batch_shader->set_uni_vec3("lightColor", glm::vec3(1.0f, 0.85f, 0.6f));
	batch_shader->set_uni_vec3("objectColor", glm::vec3(1.0f, 1.0f, 1.0f));

	if (pre_draw) pre_draw(*batch_shader);

   batch_mesh->UpdateInstanceModels(instance_models);
	batch_mesh->DrawInstanced(static_cast<GLsizei>(instance_models.size()));
}
