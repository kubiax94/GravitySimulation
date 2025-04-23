#pragma once
#ifndef MESH_H_
#define MESH_H_

#include <vector>
#include <glad/glad.h>

#include <GLFW/glfw3.h>
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
};
#endif // !MESH_H_