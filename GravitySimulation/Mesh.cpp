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

Mesh::Mesh(MeshData& mdata) : asset(asset_type::MESH)
{
	meshData = &mdata;
	set_name("Mesh: " + get_id().to_string());
	this->Init();
	status_ = asset_status::LOADED;
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

	InitInstanceBuffer();
	glBindVertexArray(0);
}

void Mesh::InitInstanceBuffer() {
	glGenBuffers(1, &instanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

	for (GLuint i = 0; i < 4; ++i) {
		glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
		glEnableVertexAttribArray(2 + i);
		glVertexAttribDivisor(2 + i, 1);
	}
}

void Mesh::UpdateInstanceModels(const std::vector<glm::mat4>& models)
{
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * models.size(), models.data(), GL_DYNAMIC_DRAW);
	glBindVertexArray(0);
}

void Mesh::Draw()
{
	glBindVertexArray(VAO);
	glDrawElements(MeshTypeToGL(type), static_cast<GLsizei>(meshData->indices.size()), GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}

void Mesh::DrawInstanced(GLsizei instanceCount)
{
	if (instanceCount <= 0)
		return;

	glBindVertexArray(VAO);
	glDrawElementsInstanced(MeshTypeToGL(type), static_cast<GLsizei>(meshData->indices.size()), GL_UNSIGNED_INT, nullptr, instanceCount);
	glBindVertexArray(0);
}

bool Mesh::is_vaild() {
    return status_ == asset_status::LOADED;
}

void Mesh::cleanup() {
    if (is_vaild()) {
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &instanceVBO);
        glDeleteVertexArrays(1, &VAO);
        status_ = asset_status::UNLOADED;
    }
}
