#pragma once
#ifndef INSTANCED_PHYSICS_SYSTEM_H
#define INSTANCED_PHYSICS_SYSTEM_H

#include "instance_buffer_manager.h"
#include "physics_data.h"
#include "compute_shader.h"
#include <vector>

/**
 * Physics system that works with instanced rendering
 * Demonstrates how compute groups can be used for batch physics updates
 */
class InstancedPhysicsSystem
{
private:
    InstanceBufferManager* instanceManager_;
    compute_shader* physicsComputeShader_;
    std::vector<physics_data> physicsData_;
    
    // Mapping between instance indices and physics data indices
    std::vector<size_t> instanceToPhysicsMap_;
    
public:
    InstancedPhysicsSystem(InstanceBufferManager* manager);
    ~InstancedPhysicsSystem();
    
    // Add physics data for an instance
    void addPhysicsObject(size_t instanceIndex, const physics_data& data);
    
    // Update physics for all objects
    void updatePhysics(float deltaTime);
    
    // Update instance positions from physics data
    void syncInstancesToPhysics();
    
    // Get physics data for debugging
    const std::vector<physics_data>& getPhysicsData() const { return physicsData_; }
};

#endif // INSTANCED_PHYSICS_SYSTEM_H