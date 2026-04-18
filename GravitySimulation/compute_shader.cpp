#include "compute_shader.h"
#include <fstream>
#include <sstream>

compute_shader::compute_shader(const char* compute_source) : shader() {
	std::string c_code = load_shader_file(compute_source);


	if (c_code.empty()) {
		std::cerr << "ERROR::COMPUTE_SHADER::FILE_EMPTY: " << compute_source << "\n";
		throw std::runtime_error("Compute shader file is empty: " + std::string(compute_source));
	}

	const char* c_shader_code = c_code.c_str();
	unsigned int compute = glCreateShader(GL_COMPUTE_SHADER);

	if (compute == 0) {
		std::cerr << "ERROR::COMPUTE_SHADER::CREATION_FAILED\n";
		throw std::runtime_error("Failed to create compute shader");
	}

	glShaderSource(compute, 1, &c_shader_code, nullptr);
	glCompileShader(compute);
	check_compiler_error(compute, "COMPUTE");
	if (status_ == asset_status::FAILED) {
		glDeleteShader(compute);
		throw std::runtime_error("Failed to compile compute shader");
	}

	this->id = glCreateProgram();

	if (this->id == 0) {
		std::cerr << "ERROR::COMPUTE_SHADER::PROGRAM_CREATION_FAILED\n";
		glDeleteShader(compute);
		throw std::runtime_error("Failed to create compute shader program");
	}

	glAttachShader(this->id, compute);
	glLinkProgram(this->id);
	check_compiler_error(this->id, "PROGRAM");
	glDeleteShader(compute);

	if (status_ == asset_status::FAILED) {
		glDeleteProgram(this->id);
		this->id = 0;
		throw std::runtime_error("Failed to link compute shader program");
	}
	
	sub_shader_type_ = shader_type::compute_shader;
	status_ = asset_status::LOADED;
}

compute_shader::~compute_shader() {
	for (auto& [binding, info] : binding_data_) {
		delete info;
	}
	binding_data_.clear();
}

void compute_shader::change_buffor_size(const size_t& n_size) {
	current_buffor_size = n_size;
}

void compute_shader::bind_ssbos(const std::vector<GLuint>& bindings) {

	for (auto bind : bindings) {
		if (binding_data_.contains(bind) && binding_data_[bind] != nullptr) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bind, binding_data_[bind]->id);
		}
	}
}

void compute_shader::dispatch(const glm::uvec3& groups) {
	if (!is_vaild() || id == 0 || groups.x == 0 || groups.y == 0 || groups.z == 0) {
		return;
	}

	use();
	glDispatchCompute(groups.x, groups.y, groups.z);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
}

void compute_shader::use() {
	shader::use();
}