#pragma once
#ifndef INSTANCE_BUFFER_MANAGER_H
#define INSTANCE_BUFFER_MANAGER_H

#include "instance_data.h"
#include <unordered_map>
#include <string>

/**
 * Manages OpenGL buffers for instanced rendering data
 */
class InstanceBufferManager
{
private:
    std::vector<InstanceData> instances_;
    std::vector<ComputeGroup> computeGroups_;
    std::vector<ShaderGroup> shaderGroups_;
    
    GLuint instanceVBO_;           // Vertex buffer for instance data
    GLuint instanceSSBO_;          // Shader storage buffer for compute operations
    bool buffersDirty_;            // Flag to track if buffers need updating
    
    void updateBuffers();
    
public:
    InstanceBufferManager();
    ~InstanceBufferManager();
    
    // Instance management
    size_t addInstance(const InstanceData& instance);
    void updateInstance(size_t index, const InstanceData& instance);
    void removeInstance(size_t index);
    size_t getInstanceCount() const { return instances_.size(); }
    
    // Group management
    size_t createComputeGroup(const std::string& name);
    size_t createShaderGroup(const std::string& name);
    void addInstanceToComputeGroup(size_t groupIndex, size_t instanceIndex);
    void addInstanceToShaderGroup(size_t groupIndex, size_t instanceIndex);
    
    // Buffer operations
    void bindInstanceVBO() const;
    void bindInstanceSSBO(GLuint binding = 0) const;
    void updateGPUBuffers();
    
    // Accessors
    const std::vector<InstanceData>& getInstances() const { return instances_; }
    const ComputeGroup& getComputeGroup(size_t index) const { return computeGroups_[index]; }
    const ShaderGroup& getShaderGroup(size_t index) const { return shaderGroups_[index]; }
    size_t getComputeGroupCount() const { return computeGroups_.size(); }
    size_t getShaderGroupCount() const { return shaderGroups_.size(); }
};

#endif // INSTANCE_BUFFER_MANAGER_H