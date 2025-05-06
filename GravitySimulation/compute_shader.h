#pragma once
#include "physics_data.h"
#include "Shader.h"

class compute_shader : public shader
{
private:
	GLuint ssbo_;
	size_t current_buffor_size;

	void change_buffor_size(const size_t& n_size);
public:
	compute_shader(const char* compute_source);

	void init();
	void use() override;
	std::vector<physics_data> compute(const size_t& size);
	void const update_ssbo(const std::vector<physics_data>& data);
};
