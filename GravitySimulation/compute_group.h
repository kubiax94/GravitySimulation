#pragma once

#include "compute_shader.h"
#include "i_data_provider.h"

class compute_group
{
	compute_shader* shader_;
	std::unordered_map<GLuint, std::vector<i_data_provider>> _binding_data;

public:
	compute_group(shader* sh);

	template <typename T>
	void add_buffer(GLuint binding, const std::vector<T>& data);

	template <typename T>
	void update_buffer(GLuint binding, const std::vector<T>& data);
	void bind_all();


	template <typename T>
	std::vector<T> get_buffer_data(GLuint binding) const;

	void dipatch(const glm::uvec3& groups);
	void attach_shader(compute_shader* shader);
	
};

template <typename T>
void compute_group::add_buffer(GLuint binding, const std::vector<T>& data) {

	if (!_binding_data.contains(binding))
		_binding_data[binding] = data;
}
