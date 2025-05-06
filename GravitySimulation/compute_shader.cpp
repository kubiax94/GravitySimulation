#include "compute_shader.h"

void compute_shader::change_buffor_size(const size_t& n_size) {
	current_buffor_size = n_size;
}

compute_shader::compute_shader(const char* compute_source) : shader() {
	std::string c_code;
	std::ifstream c_file;

	c_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try
	{
		c_file.open(compute_source);
		std::stringstream c_shader_stream;

		c_shader_stream << c_file.rdbuf();

		c_file.close();

		c_code = c_shader_stream.str();
	} catch (std::ifstream::failure& e) {
		std::cout << "ERROR::COMPUTE_SHADER::FILE_NOT_SUCCESFULLY_READ: " << e.what() << std::endl;
	}

	const char* c_shader_code = c_code.c_str();
	unsigned int compute;

	compute = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute, 1, &c_shader_code, NULL);
	glCompileShader(compute);
	check_compiler_error(compute, "COMPUTE");

	this->id = glCreateProgram();
	glAttachShader(id, compute);
	glLinkProgram(id);
	check_compiler_error(compute, "PROGRAM");

	glDeleteShader(compute);
	
	type_ = shader_type::compute_shader;

	glGenBuffers(1, &ssbo_);
	
}

void compute_shader::use() {
	shader::use();
}

std::vector<physics_data> compute_shader::compute(const size_t& size) {
	shader::use();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
	glDispatchCompute(size, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	//auto* p_data = static_cast<physics_data*>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY));
	

	std::vector<physics_data> result(size);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(physics_data) * size, result.data());
	//if (p_data)
	//	std::copy(p_data, p_data + size, result.begin());

	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

	return result;
}

void const compute_shader::update_ssbo(const std::vector<physics_data>& data) {

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);

	if (current_buffor_size != data.size()) {
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(physics_data) * data.size(), data.data(), GL_DYNAMIC_DRAW);
		change_buffor_size(data.size());
	}
	else {
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(physics_data) * data.size(), data.data());
	}

	//Aby w kodzie shadera był widoczy dla layout
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);

}
