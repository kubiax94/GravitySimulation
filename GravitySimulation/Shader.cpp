#include "Shader.h"

unsigned int shader::current_shader_id_ = 0;

shader::shader(const char* vertex_source, const char* fragment_source) {

	std::string vertexCode;
	std::string fragmentCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;

	vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try {
		vShaderFile.open(vertex_source);
		fShaderFile.open(fragment_source);
		std::stringstream vShaderStream, fShaderStream;

		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();

		vShaderFile.close();
		fShaderFile.close();

		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();
	}
	catch (std::ifstream::failure& e) {
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << e.what() << std::endl;
	}

	const char* vShaderCode = vertexCode.c_str();
	const char* fShaderCode = fragmentCode.c_str();

	unsigned int vertex, fragment;

	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vShaderCode, NULL);
	glCompileShader(vertex);
	check_compiler_error(vertex, "VERTEX");

	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fShaderCode, NULL);
	glCompileShader(fragment);
	check_compiler_error(fragment, "FRAGMENT");


	this->id = glCreateProgram();
	glAttachShader(id, vertex);
	glAttachShader(id, fragment);
	glLinkProgram(id);
	check_compiler_error(id, "PROGRAM");

	glDeleteShader(vertex);
	glDeleteShader(fragment);

}
void shader::use() {
	if (shader::current_shader_id_ != id) {
		shader::current_shader_id_ = id;
		glUseProgram(this->id);
	}
}
void shader::check_compiler_error(GLuint shader, const std::string& type) {
	GLint success;
	GLchar infoLog[1024];
	if (type != "PROGRAM") {
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

		if (!success) {
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n--------------------------------------------------" << std::endl;
		}

	}
	else {
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cout << "ERROR::SHADER_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n--------------------------------------------------" << std::endl;
		}
	}
}

void shader::set_uni_float(const std::string& u_name, const float& x) {
	if (!registered_uniforms_.contains(u_name))
		register_uniform(u_name);

	glUniform1f(registered_uniforms_[u_name], x);
}
void shader::set_uni_vec2(const std::string& u_name, const glm::vec2& n_vec2) {
	if (!registered_uniforms_.contains(u_name))
		register_uniform(u_name);

	glUniform2fv(registered_uniforms_[u_name], 1, &n_vec2[0]);
}
void shader::set_uni_vec2(const std::string& u_name, const float& x, const float& y) {
	if (!registered_uniforms_.contains(u_name))
		register_uniform(u_name);

	glUniform2f(registered_uniforms_[u_name], x, y);
}
void shader::set_uni_vec3(const std::string& u_name, const glm::vec3& n_vec3) {
	if (!registered_uniforms_.contains(u_name))
		register_uniform(u_name);

	glUniform3fv(registered_uniforms_[u_name], 1, &n_vec3[0]);
}
void shader::set_uni_vec3(const std::string& u_name, const float& x, const float& y, const float& z) {
	if (!registered_uniforms_.contains(u_name))
		register_uniform(u_name);

	glUniform3f(registered_uniforms_[u_name], x, y, z);
}
void shader::set_uni_vec4(const std::string& u_name, const glm::vec4& n_vec4) {
	if (!registered_uniforms_.contains(u_name))
		register_uniform(u_name);

	glUniform4fv(registered_uniforms_[u_name], 1, &n_vec4[0]);
}
void shader::set_uni_vec4(const std::string& u_name, const float& x, const float& y, const float& z, const float& a) {
	if (!registered_uniforms_.contains(u_name))
		register_uniform(u_name);

	glUniform4f(registered_uniforms_[u_name], x, y, z, a);
}

void shader::set_uniform_mat4(const std::string &u_name, const glm::mat4& n_mat4) {
	if (!registered_uniforms_.contains(u_name)) {
		register_uniform(u_name);
	}
	glUniformMatrix4fv(registered_uniforms_.at(u_name), 1, GL_FALSE, &n_mat4[0][0]);
}
void shader::register_uniform(const std::string& u_name) {
	registered_uniforms_[u_name] = glGetUniformLocation(this->id, u_name.c_str());
}

