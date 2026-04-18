#pragma once
#ifndef SHADER_H
#define SHADER_H


#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <iostream>
#include <unordered_map>

#include "asset.h"

class shader : public asset
{

protected:
	static unsigned int current_shader_id_;
	void check_compiler_error(GLuint shader, const std::string& type);

	std::unordered_map<std::string, GLint> registered_uniforms_;
	// registers a uniform location into the map
	void register_uniform(const std::string& u_name);
	shader_type sub_shader_type_;
	//TODO: Przenieœc resource managera
	std::string  load_shader_file(const char* shader_source);
public:
	shader();
	shader(const char* vertex_source, const char* fragment_source);
	~shader() override = default;
	virtual void use();
	//Shader program ID
	unsigned int id;

	//DUZO FUNKCJI DO SETOWANIA UNIFORMoW
	void set_uni_float(const std::string& u_name, const float& x);
	void set_uni_int(const std::string& u_name, const int& x);
	void set_uni_vec2(const std::string& u_name, const glm::vec2& n_vec2);
	void set_uni_vec2(const std::string& u_name, const float& x, const float& y);
	void set_uni_vec3(const std::string& u_name, const glm::vec3& n_vec3);
	void set_uni_vec3(const std::string& u_name, const float& x, const float& y, const float& z);
	void set_uni_vec4(const std::string& u_name, const glm::vec4& n_vec4);
	void set_uni_vec4(const std::string& u_name, const float& x, const float& y, const float& z, const float& a);

	void set_uniform_mat4(const std::string& u_name, const glm::mat4& n_mat4);

	void cleanup() override;
	bool is_vaild() override;
};

#endif // !_SHADER_
