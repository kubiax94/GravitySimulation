#include "Mesh.h"

#include <cstring>

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
 instance_buffer_capacity_ = 1;
 glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * instance_buffer_capacity_, nullptr, GL_STREAM_DRAW);

	for (GLuint i = 0; i < 4; ++i) {
		glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
		glEnableVertexAttribArray(2 + i);
		glVertexAttribDivisor(2 + i, 1);
	}

	glGenBuffers(1, &instancePhysicsIndexVBO);
	glBindBuffer(GL_ARRAY_BUFFER, instancePhysicsIndexVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLint) * instance_buffer_capacity_, nullptr, GL_STREAM_DRAW);
	glVertexAttribIPointer(6, 1, GL_INT, sizeof(GLint), nullptr);
	glEnableVertexAttribArray(6);
	glVertexAttribDivisor(6, 1);
}

bool Mesh::AreInstanceModelsUnchanged(const std::vector<glm::mat4>& models) const {
	if (cached_instance_models_.size() != models.size())
		return false;

	if (models.empty())
		return true;

	return std::memcmp(cached_instance_models_.data(), models.data(), sizeof(glm::mat4) * models.size()) == 0;
}

bool Mesh::AreInstancePhysicsIndicesUnchanged(const std::vector<int>& indices) const {
	return cached_instance_physics_indices_ == indices;
}

void Mesh::UpdateInstanceModels(const std::vector<glm::mat4>& models)
{
 if (AreInstanceModelsUnchanged(models))
		return;

	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

	if (models.size() > instance_buffer_capacity_) {
		while (instance_buffer_capacity_ < models.size())
			instance_buffer_capacity_ *= 2;
	}

	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * instance_buffer_capacity_, nullptr, GL_STREAM_DRAW);

	if (!models.empty())
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4) * models.size(), models.data());

	cached_instance_models_ = models;
}

void Mesh::UpdateInstancePhysicsIndices(const std::vector<int>& indices)
{
	if (AreInstancePhysicsIndicesUnchanged(indices))
		return;

	glBindBuffer(GL_ARRAY_BUFFER, instancePhysicsIndexVBO);

	if (indices.size() > instance_buffer_capacity_) {
		while (instance_buffer_capacity_ < indices.size())
			instance_buffer_capacity_ *= 2;
	}

	glBufferData(GL_ARRAY_BUFFER, sizeof(GLint) * instance_buffer_capacity_, nullptr, GL_STREAM_DRAW);

	if (!indices.empty())
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GLint) * indices.size(), indices.data());

	cached_instance_physics_indices_ = indices;
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
      glDeleteBuffers(1, &instancePhysicsIndexVBO);
        glDeleteVertexArrays(1, &VAO);
        status_ = asset_status::UNLOADED;
    }
}
