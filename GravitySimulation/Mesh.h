#pragma once
#ifndef MESH_H_
#define MESH_H_

#include <memory>
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
 protected:
	GLuint VAO{}, VBO{}, EBO{};
	GLuint instanceVBO{};
	GLuint instancePhysicsIndexVBO{};
    size_t instance_buffer_capacity_ = 0;
 bool gpu_initialized_ = false;
	std::vector<glm::mat4> cached_instance_models_;
    std::vector<int> cached_instance_physics_indices_;
	void Init();
	void InitInstanceBuffer();
	[[nodiscard]] bool AreInstanceModelsUnchanged(const std::vector<glm::mat4>& models) const;
	[[nodiscard]] bool AreInstancePhysicsIndicesUnchanged(const std::vector<int>& indices) const;
 [[nodiscard]] bool HasValidMeshData() const;
	std::shared_ptr<MeshData> mesh_data_;

public:
	void Draw();
	void DrawInstanced(GLsizei instanceCount);
	void UpdateInstanceModels(const std::vector<glm::mat4>& models);
	void UpdateInstancePhysicsIndices(const std::vector<int>& indices);
  Mesh();
	Mesh(MeshData& mdata);
    Mesh(std::shared_ptr<MeshData> mdata);
	MeshType type = MeshType::TRIANGLES;

	void set_mesh_data(const MeshData& mdata);
	void set_mesh_data(MeshData&& mdata);
	void set_mesh_data(std::shared_ptr<MeshData> mdata);
	const std::shared_ptr<MeshData>& get_mesh_data() const;
	bool finalize() override;

	bool is_vaild() override;
	void cleanup() override;
};
#endif // !MESH_H_