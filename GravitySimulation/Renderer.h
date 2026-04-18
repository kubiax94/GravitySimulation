#pragma once

#ifndef RENDERER_H
#define RENDERER_H

#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"

class renderer : public transformable
{
private:
	shader* shader_;
	Mesh* mesh_;

	glm::vec3 visual_scale_ = glm::vec3(5.f);

public:
	[[nodiscard]] type_id_t get_type_id() const override;
	float renderer_scale = 1.f;
	renderer(scene_node* s_node, shader* shader, Mesh* mesh);
	renderer(scene_node* s_node, shader* shader, Mesh* mesh, const float& v_scale);
	glm::mat4 get_visual_model_matrix() const;
	void initialize();
	void set_visual_scale(const glm::vec3& scalar);
	void attach_to(scene_node* n_node) override;
	bool detach() override;
	void draw(Camera* c, const std::function<void(shader&)>& pre_draw = nullptr) const;
	shader* get_shader() const { return shader_; }
	Mesh* get_mesh() const { return mesh_; }
};
#endif
