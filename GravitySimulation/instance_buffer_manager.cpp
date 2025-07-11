#include "instance_buffer_manager.h"
#include <iostream>
#include <algorithm>

InstanceBufferManager::InstanceBufferManager()
    : instanceVBO_(0), instanceSSBO_(0), buffersDirty_(true)
{
    glGenBuffers(1, &instanceVBO_);
    glGenBuffers(1, &instanceSSBO_);
}

InstanceBufferManager::~InstanceBufferManager()
{
    if (instanceVBO_ != 0) {
        glDeleteBuffers(1, &instanceVBO_);
    }
    if (instanceSSBO_ != 0) {
        glDeleteBuffers(1, &instanceSSBO_);
    }
}

size_t InstanceBufferManager::addInstance(const InstanceData& instance)
{
    instances_.push_back(instance);
    buffersDirty_ = true;
    return instances_.size() - 1;
}

void InstanceBufferManager::updateInstance(size_t index, const InstanceData& instance)
{
    if (index < instances_.size()) {
        instances_[index] = instance;
        buffersDirty_ = true;
    }
}

void InstanceBufferManager::removeInstance(size_t index)
{
    if (index < instances_.size()) {
        instances_.erase(instances_.begin() + index);
        buffersDirty_ = true;
        
        // Update indices in groups
        for (auto& group : computeGroups_) {
            auto& indices = group.instanceIndices;
            indices.erase(std::remove_if(indices.begin(), indices.end(),
                [index](size_t i) { return i == index; }), indices.end());
            
            // Adjust indices that are greater than the removed index
            for (auto& i : indices) {
                if (i > index) i--;
            }
        }
        
        for (auto& group : shaderGroups_) {
            auto& indices = group.instanceIndices;
            indices.erase(std::remove_if(indices.begin(), indices.end(),
                [index](size_t i) { return i == index; }), indices.end());
            
            // Adjust indices that are greater than the removed index
            for (auto& i : indices) {
                if (i > index) i--;
            }
        }
    }
}

size_t InstanceBufferManager::createComputeGroup(const std::string& name)
{
    computeGroups_.emplace_back(name);
    return computeGroups_.size() - 1;
}

size_t InstanceBufferManager::createShaderGroup(const std::string& name)
{
    shaderGroups_.emplace_back(name);
    return shaderGroups_.size() - 1;
}

void InstanceBufferManager::addInstanceToComputeGroup(size_t groupIndex, size_t instanceIndex)
{
    if (groupIndex < computeGroups_.size() && instanceIndex < instances_.size()) {
        auto& indices = computeGroups_[groupIndex].instanceIndices;
        if (std::find(indices.begin(), indices.end(), instanceIndex) == indices.end()) {
            indices.push_back(instanceIndex);
        }
    }
}

void InstanceBufferManager::addInstanceToShaderGroup(size_t groupIndex, size_t instanceIndex)
{
    if (groupIndex < shaderGroups_.size() && instanceIndex < instances_.size()) {
        auto& indices = shaderGroups_[groupIndex].instanceIndices;
        if (std::find(indices.begin(), indices.end(), instanceIndex) == indices.end()) {
            indices.push_back(instanceIndex);
        }
    }
}

void InstanceBufferManager::bindInstanceVBO() const
{
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
}

void InstanceBufferManager::bindInstanceSSBO(GLuint binding) const
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, instanceSSBO_);
}

void InstanceBufferManager::updateGPUBuffers()
{
    if (buffersDirty_ && !instances_.empty()) {
        updateBuffers();
        buffersDirty_ = false;
    }
}

void InstanceBufferManager::updateBuffers()
{
    // Update VBO for vertex shader instancing
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    glBufferData(GL_ARRAY_BUFFER, 
                 instances_.size() * sizeof(InstanceData), 
                 instances_.data(), 
                 GL_DYNAMIC_DRAW);
    
    // Update SSBO for compute shader operations
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, instanceSSBO_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 instances_.size() * sizeof(InstanceData), 
                 instances_.data(), 
                 GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}