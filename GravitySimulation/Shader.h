#pragma once
#ifndef SHADER_H
#define SHADER_H


#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

enum class shader_type : std::uint8_t
{
	visual_shader,
	compute_shader
};

class shader
{
protected:
	static unsigned int current_shader_id_;
	std::unordered_map<std::string, GLint> registered_uniforms_;
	void check_compiler_error(GLuint shader, const std::string& type);
	void register_uniform(const std::string& u_name);
	shader_type type_;

public:
	virtual ~shader() = default;
	//Shader program ID
	unsigned int id;

	const shader_type& get_type() const;
	shader(const char* vertex_source, const char* fragment_source);
	virtual void use();

	//DUZO FUNKCJI DO SETOWANIA UNIFORMoW
	void set_uni_float(const std::string& u_name, const float& x);
	void set_uni_vec2(const std::string& u_name, const glm::vec2& n_vec2);
	void set_uni_vec2(const std::string& u_name, const float& x, const float& y);
	void set_uni_vec3(const std::string& u_name, const glm::vec3& n_vec3);
	void set_uni_vec3(const std::string& u_name, const float& x, const float& y, const float& z);
	void set_uni_vec4(const std::string& u_name, const glm::vec4& n_vec4);
	void set_uni_vec4(const std::string& u_name, const float& x, const float& y, const float& z, const float& a);

	void set_uniform_mat4(const std::string& u_name, const glm::mat4& n_mat4);
};

#endif // !_SHADER_
