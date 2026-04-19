#pragma once
#ifndef MESH_H_
#define MESH_H_

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "asset.h"

enum class MeshType {
	TRIANGLES,
	LINES,
	POINTS
};

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TextCoords;
};

struct MeshData {
	std::vector<Vertex> vertecies;
	std::vector<unsigned int> indices;
};

class Mesh : public asset
{
	GLuint VAO{}, VBO{}, EBO{};
	GLuint instanceVBO{};
    size_t instance_buffer_capacity_ = 0;
	std::vector<glm::mat4> cached_instance_models_;
	void Init();
	void InitInstanceBuffer();
 [[nodiscard]] bool AreInstanceModelsUnchanged(const std::vector<glm::mat4>& models) const;
	MeshData* meshData;

public:
	void Draw();
	void DrawInstanced(GLsizei instanceCount);
	void UpdateInstanceModels(const std::vector<glm::mat4>& models);
	Mesh(MeshData& mdata);
	MeshType type = MeshType::TRIANGLES;

	bool is_vaild() override;
	void cleanup() override;
};
#endif // !MESH_H_