#pragma once
#ifndef INSTANCE_DATA_H
#define INSTANCE_DATA_H

#include <glm/glm.hpp>
#include <vector>
#include <glad/glad.h>

/**
 * Structure to hold per-instance data for instanced rendering
 */
struct InstanceData
{
    glm::mat4 modelMatrix;      // Transformation matrix for this instance
    glm::vec3 color;            // Color for this instance
    float scale;                // Additional scale factor
    
    InstanceData() : modelMatrix(1.0f), color(1.0f), scale(1.0f) {}
    InstanceData(const glm::mat4& model, const glm::vec3& col, float sc = 1.0f) 
        : modelMatrix(model), color(col), scale(sc) {}
};

/**
 * Structure to manage groups of similar instances for compute operations
 */
struct ComputeGroup
{
    std::string name;
    std::vector<size_t> instanceIndices;  // Indices into main instance array
    GLuint computeProgram;                 // Compute shader for this group
    
    ComputeGroup(const std::string& groupName) : name(groupName), computeProgram(0) {}
};

/**
 * Structure to manage shader programs for different rendering groups
 */
struct ShaderGroup
{
    std::string name;
    GLuint shaderProgram;                  // Shader program for this group
    std::vector<size_t> instanceIndices;  // Instances using this shader
    
    ShaderGroup(const std::string& groupName) : name(groupName), shaderProgram(0) {}
};

#endif // INSTANCE_DATA_H