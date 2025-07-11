#pragma once
#ifndef INSTANCED_RENDERER_H
#define INSTANCED_RENDERER_H

#include "Renderer.h"
#include "instance_buffer_manager.h"
#include "Mesh.h"
#include "Camera.h"

/**
 * Renderer that handles instanced drawing of multiple objects with a single draw call
 */
class InstancedRenderer
{
private:
    shader* shader_;
    Mesh* mesh_;
    InstanceBufferManager* instanceManager_;
    GLuint instanceVAO_;
    
    void setupInstancedVAO();
    
public:
    InstancedRenderer(shader* shaderProgram, Mesh* mesh, InstanceBufferManager* manager);
    ~InstancedRenderer();
    
    // Rendering
    void drawInstanced(Camera* camera, size_t shaderGroupIndex, 
                      const std::function<void(shader&)>& preDrawCallback = nullptr);
    void drawAllInstances(Camera* camera, 
                         const std::function<void(shader&)>& preDrawCallback = nullptr);
    
    // Utility
    void updateInstanceBuffers();
    
    // Accessors
    shader* getShader() const { return shader_; }
    Mesh* getMesh() const { return mesh_; }
};

#endif // INSTANCED_RENDERER_H