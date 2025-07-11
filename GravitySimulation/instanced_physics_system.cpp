#include "instanced_physics_system.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

InstancedPhysicsSystem::InstancedPhysicsSystem(InstanceBufferManager* manager)
    : instanceManager_(manager), physicsComputeShader_(nullptr)
{
    // Initialize compute shader for physics (using existing gravity simulation shader)
    physicsComputeShader_ = new compute_shader("C:/Users/Kubiaxx/source/repos/GravitySimulation/GravitySimulation/gravity_simulation.glsl");
}

InstancedPhysicsSystem::~InstancedPhysicsSystem()
{
    delete physicsComputeShader_;
}

void InstancedPhysicsSystem::addPhysicsObject(size_t instanceIndex, const physics_data& data)
{
    physicsData_.push_back(data);
    
    // Ensure mapping vector is large enough
    if (instanceToPhysicsMap_.size() <= instanceIndex) {
        instanceToPhysicsMap_.resize(instanceIndex + 1, SIZE_MAX);
    }
    
    instanceToPhysicsMap_[instanceIndex] = physicsData_.size() - 1;
}

void InstancedPhysicsSystem::updatePhysics(float deltaTime)
{
    if (physicsData_.empty()) return;
    
    // Update SSBO with current physics data
    physicsComputeShader_->update_ssbo(physicsData_);
    
    // Set uniforms for physics computation
    physicsComputeShader_->use();
    physicsComputeShader_->set_uni_float("G", 6.67430e-11f); // Gravitational constant
    
    // Dispatch compute shader
    auto result = physicsComputeShader_->compute(physicsData_.size());
    
    // Update physics data with results
    if (result.size() == physicsData_.size()) {
        physicsData_ = result;
    }
    
    // Apply velocity integration (Euler method for simplicity)
    for (auto& data : physicsData_) {
        // Update velocity: v = v + a*dt
        glm::vec3 acceleration = data.accumulated_force / data.position.w; // F/m
        data.velocity += glm::vec4(acceleration * deltaTime, 0.0f);
        
        // Update position: p = p + v*dt
        data.position += data.velocity * deltaTime;
        
        // Reset accumulated force
        data.accumulated_force = glm::vec4(0.0f);
    }
}

void InstancedPhysicsSystem::syncInstancesToPhysics()
{
    const auto& instances = instanceManager_->getInstances();
    
    for (size_t i = 0; i < instances.size(); ++i) {
        if (i < instanceToPhysicsMap_.size() && instanceToPhysicsMap_[i] != SIZE_MAX) {
            size_t physicsIndex = instanceToPhysicsMap_[i];
            if (physicsIndex < physicsData_.size()) {
                // Create new instance data with updated position
                InstanceData updatedInstance = instances[i];
                
                // Extract position from physics data
                glm::vec3 position = glm::vec3(physicsData_[physicsIndex].position);
                
                // Update transformation matrix with new position
                // Preserve existing rotation and scale, just update translation
                glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
                
                // For simplicity, assume no rotation (could extract from current matrix)
                updatedInstance.modelMatrix = translation;
                
                // Update instance in buffer
                instanceManager_->updateInstance(i, updatedInstance);
            }
        }
    }
    
    // Update GPU buffers with new positions
    instanceManager_->updateGPUBuffers();
}