#pragma once

#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/mat4x4.hpp>

#include "Renderer.h"

class instance_manager
{
	struct batch_key {
		shader* shader_ptr{};
		Mesh* mesh_ptr{};

		bool operator==(const batch_key& other) const {
			return shader_ptr == other.shader_ptr && mesh_ptr == other.mesh_ptr;
		}
	};

	struct batch_key_hash {
		size_t operator()(const batch_key& key) const {
			return std::hash<void*>{}(key.shader_ptr) ^ (std::hash<void*>{}(key.mesh_ptr) << 1);
		}
	};

public:
	void draw_instanced(const std::vector<renderer*>& renderers,
		Camera* camera,
		const std::function<void(shader&)>& pre_draw = nullptr) const;
};

