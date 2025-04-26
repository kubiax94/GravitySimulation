#include "Mesh.h"

GLenum MeshTypeToGL(MeshType t)
{
	switch (t)
	{
	case MeshType::TRIANGLES: return GL_TRIANGLES;
	case MeshType::LINES: return GL_LINES;
	case MeshType::POINTS: return GL_POINTS;
	default: return GL_TRIANGLES;
	}
}

Mesh::Mesh(MeshData& mdata)
{
	meshData = &mdata;
	this->Init();
}

void Mesh::Init()
{
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * meshData->vertecies.size(), meshData->vertecies.data(),
	             GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * meshData->indices.size(), meshData->indices.data(),
	             GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
	glEnableVertexAttribArray(1);
}

void Mesh::Draw()
{
	glBindVertexArray(VAO);
	glDrawElements(MeshTypeToGL(type), meshData->indices.size(), GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}
