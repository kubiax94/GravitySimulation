#pragma once
#ifndef MESH_H_
#define MESH_H_

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

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

class Mesh
{
private:
	GLuint VAO, VBO, EBO;
	void Init();
	MeshData* meshData;

public:
	void Draw();
	Mesh(MeshData& mdata);
	MeshType type = MeshType::TRIANGLES;
	
	// Accessors for instanced rendering
	size_t getIndexCount() const { return meshData->indices.size(); }
	GLuint getVAO() const { return VAO; }
	GLuint getVBO() const { return VBO; }
	GLuint getEBO() const { return EBO; }
};
#endif // !MESH_H_