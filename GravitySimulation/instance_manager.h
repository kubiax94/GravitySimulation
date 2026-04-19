#pragma once

#include <glad/glad.h>
#include <vector>
#include <functional>

#include "Renderer.h"

class instance_manager
{
public:
    void draw_instanced(shader* batch_shader,
		Mesh* batch_mesh,
		const std::vector<renderer*>& renderers,
        const std::vector<glm::mat4>& instance_models,
		const std::vector<int>& instance_physics_indices,
		const render_frame_context& frame_context,
		GLuint physics_ssbo = 0,
		int instance_base_index = -1,
		bool use_gpu_positions = false,
		const std::function<void(shader&)>& pre_draw = nullptr) const;
};

