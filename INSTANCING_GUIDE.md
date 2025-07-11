# OpenGL Instancing Implementation Guide

This implementation adds OpenGL instancing capabilities to the gravity simulation using compute groups and shader groups as requested.

## Overview

The instancing system allows rendering multiple similar objects (like planets) with a single draw call, dramatically improving performance when dealing with many objects.

## Key Components

### 1. Instance Data Structure (`instance_data.h`)
- `InstanceData`: Holds per-instance transformation matrix, color, and scale
- `ComputeGroup`: Manages groups of instances for compute operations
- `ShaderGroup`: Manages groups of instances for rendering with specific shaders

### 2. Instance Buffer Manager (`instance_buffer_manager.h/cpp`)
- Manages OpenGL buffers for instance data
- Provides VBO for vertex shader instancing
- Provides SSBO for compute shader operations
- Organizes instances into compute and shader groups

### 3. Instanced Renderer (`instanced_renderer.h/cpp`)
- Handles instanced drawing with `glDrawElementsInstanced`
- Sets up VAO with both vertex data and instance data
- Can render specific shader groups or all instances

### 4. Instanced Shaders
- `instanced.vs.shader`: Vertex shader with per-instance attributes
- `instanced.fs.shader`: Fragment shader supporting per-instance colors

## Usage Example

```cpp
// Create instance manager
InstanceBufferManager* instanceManager = new InstanceBufferManager();

// Create instanced renderer
shader* instancedShader = new shader("instanced.vs.shader", "instanced.fs.shader");
InstancedRenderer* renderer = new InstancedRenderer(instancedShader, mesh, instanceManager);

// Create shader group
size_t planetGroup = instanceManager->createShaderGroup("planets");

// Add instances
for (each planet) {
    InstanceData instance;
    instance.modelMatrix = transformMatrix;
    instance.color = planetColor;
    instance.scale = planetScale;
    
    size_t index = instanceManager->addInstance(instance);
    instanceManager->addInstanceToShaderGroup(planetGroup, index);
}

// Update GPU buffers
instanceManager->updateGPUBuffers();

// Render all instances in one draw call
renderer->drawAllInstances(camera, [&](shader& s) {
    // Set uniforms like lighting
    s.set_uni_vec3("lightPos", lightPosition);
    s.set_uni_vec3("lightColor", lightColor);
});
```

## Integration with Existing Code

### Minimal Changes Approach

1. **Keep existing individual renderers** for special objects (like the sun)
2. **Replace planet rendering loop** with instanced rendering
3. **Maintain physics system** - instances are purely for rendering

### Before (Individual Rendering):
```cpp
for (auto* render : planet_renders) {
    render->draw(cam, [&](shader& s) {
        // Set uniforms for each planet
    });
}
```

### After (Instanced Rendering):
```cpp
// Single draw call for all planets
planetRenderer->drawAllInstances(cam, [&](shader& s) {
    // Set uniforms once for all planets
});
```

## Compute Groups Usage

Compute groups organize instances for compute operations:

```cpp
// Create compute group for physics calculations
size_t physicsGroup = instanceManager->createComputeGroup("physics");

// Add instances that need physics updates
instanceManager->addInstanceToComputeGroup(physicsGroup, instanceIndex);

// Use in compute shaders for batch operations
const ComputeGroup& group = instanceManager->getComputeGroup(physicsGroup);
// Process group.instanceIndices for compute operations
```

## Shader Groups Usage

Shader groups organize instances by rendering requirements:

```cpp
// Different shader groups for different materials/effects
size_t rockyPlanets = instanceManager->createShaderGroup("rocky");
size_t gasGiants = instanceManager->createShaderGroup("gas");

// Render each group with appropriate shaders
planetRenderer->drawInstanced(camera, rockyPlanets, rockyShaderCallback);
gasRenderer->drawInstanced(camera, gasGiants, gasShaderCallback);
```

## Performance Benefits

1. **Reduced Draw Calls**: N planets rendered with 1 call instead of N calls
2. **GPU Efficiency**: Better GPU utilization with instanced drawing
3. **Memory Efficiency**: Shared vertex data, per-instance attributes only
4. **Batch Operations**: Compute groups enable batch physics/animation updates

## Files Added

- `instance_data.h` - Core data structures
- `instance_buffer_manager.h/cpp` - Buffer management
- `instanced_renderer.h/cpp` - Instanced rendering
- `instanced.vs.shader` - Instanced vertex shader
- `instanced.fs.shader` - Instanced fragment shader  
- `instanced_simulation_test.h/cpp` - Example integration
- `instanced_main_example.cpp` - Usage demonstration

## Benefits for Gravity Simulation

1. **Scalability**: Can handle thousands of objects efficiently
2. **Organization**: Compute groups for physics, shader groups for rendering
3. **Flexibility**: Mix instanced and individual rendering as needed
4. **Performance**: Dramatic improvement for large numbers of similar objects