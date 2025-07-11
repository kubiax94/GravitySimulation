#include "instanced_renderer.h"
#include <iostream>

InstancedRenderer::InstancedRenderer(shader* shaderProgram, Mesh* mesh, InstanceBufferManager* manager)
    : shader_(shaderProgram), mesh_(mesh), instanceManager_(manager), instanceVAO_(0)
{
    setupInstancedVAO();
}

InstancedRenderer::~InstancedRenderer()
{
    if (instanceVAO_ != 0) {
        glDeleteVertexArrays(1, &instanceVAO_);
    }
}

void InstancedRenderer::setupInstancedVAO()
{
    glGenVertexArrays(1, &instanceVAO_);
    glBindVertexArray(instanceVAO_);
    
    // Bind the mesh's vertex buffer and set up standard vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, mesh_->getVBO());
    
    // Position attribute (location 0) - matches existing vertex shader
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
    glEnableVertexAttribArray(0);
    
    // Normal attribute (location 1) - matches existing vertex shader
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    glEnableVertexAttribArray(1);
    
    // Bind element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_->getEBO());
    
    // Now set up instance data attributes
    instanceManager_->bindInstanceVBO();
    
    // Model matrix (mat4 takes 4 attribute locations: 2, 3, 4, 5)
    for (int i = 0; i < 4; i++) {
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, 
                             sizeof(InstanceData), 
                             (void*)(sizeof(glm::vec4) * i));
        glEnableVertexAttribArray(2 + i);
        glVertexAttribDivisor(2 + i, 1); // Advance once per instance
    }
    
    // Color attribute (location 6)
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, 
                         sizeof(InstanceData), 
                         (void*)offsetof(InstanceData, color));
    glEnableVertexAttribArray(6);
    glVertexAttribDivisor(6, 1);
    
    // Scale attribute (location 7)
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, 
                         sizeof(InstanceData), 
                         (void*)offsetof(InstanceData, scale));
    glEnableVertexAttribArray(7);
    glVertexAttribDivisor(7, 1);
    
    glBindVertexArray(0);
}

void InstancedRenderer::drawInstanced(Camera* camera, size_t shaderGroupIndex, 
                                     const std::function<void(shader&)>& preDrawCallback)
{
    if (shaderGroupIndex >= instanceManager_->getShaderGroupCount()) {
        return;
    }
    
    const auto& shaderGroup = instanceManager_->getShaderGroup(shaderGroupIndex);
    if (shaderGroup.instanceIndices.empty()) {
        return;
    }
    
    // Update buffers if needed
    instanceManager_->updateGPUBuffers();
    
    // Use shader and set up matrices
    shader_->use();
    
    auto projection = camera->GetProjectionMatrix(1280.0f / 720.0f);
    auto view = camera->GetViewMatrix();
    
    shader_->set_uniform_mat4("view", view);
    shader_->set_uniform_mat4("projection", projection);
    
    // Call pre-draw callback for additional uniforms
    if (preDrawCallback) {
        preDrawCallback(*shader_);
    }
    
    // Bind our instanced VAO
    glBindVertexArray(instanceVAO_);
    
    // Draw instances for this shader group
    GLsizei instanceCount = static_cast<GLsizei>(shaderGroup.instanceIndices.size());
    
    // For now, draw all instances - in a more advanced implementation,
    // you'd create sub-buffers or use indirect drawing for specific groups
    GLsizei indexCount = static_cast<GLsizei>(mesh_->getIndexCount());
    glDrawElementsInstanced(GL_TRIANGLES, 
                           indexCount, 
                           GL_UNSIGNED_INT, 
                           0, 
                           instanceCount);
    
    glBindVertexArray(0);
}

void InstancedRenderer::drawAllInstances(Camera* camera, 
                                        const std::function<void(shader&)>& preDrawCallback)
{
    size_t totalInstances = instanceManager_->getInstanceCount();
    if (totalInstances == 0) {
        return;
    }
    
    // Update buffers if needed
    instanceManager_->updateGPUBuffers();
    
    // Use shader and set up matrices
    shader_->use();
    
    auto projection = camera->GetProjectionMatrix(1280.0f / 720.0f);
    auto view = camera->GetViewMatrix();
    
    shader_->set_uniform_mat4("view", view);
    shader_->set_uniform_mat4("projection", projection);
    
    // Call pre-draw callback for additional uniforms
    if (preDrawCallback) {
        preDrawCallback(*shader_);
    }
    
    // Bind our instanced VAO
    glBindVertexArray(instanceVAO_);
    
    // Draw all instances
    GLsizei instanceCount = static_cast<GLsizei>(totalInstances);
    GLsizei indexCount = static_cast<GLsizei>(mesh_->getIndexCount());
    glDrawElementsInstanced(GL_TRIANGLES, 
                           indexCount, 
                           GL_UNSIGNED_INT, 
                           0, 
                           instanceCount);
    
    glBindVertexArray(0);
}

void InstancedRenderer::updateInstanceBuffers()
{
    instanceManager_->updateGPUBuffers();
}