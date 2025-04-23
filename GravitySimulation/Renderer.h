#pragma once

#ifndef RENDERER_H
#define RENDERER_H

#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"

class renderer : public transformable
{
private:
	//For OpenGL to draw vertex
	shader* shader_;
	Mesh* mesh_;

public:
	[[nodiscard]] type_id_t get_type_id() const override;

	renderer(scene_node* s_node, shader* shader, Mesh* mesh);

	void initialize();
	void draw(Camera* c, const std::function<void(shader&)>& pre_draw = nullptr) const;


};
#endif
