#version 460 core

// Per-vertex attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// Per-instance attributes
layout (location = 2) in mat4 aInstanceMatrix;  // Locations 2, 3, 4, 5
layout (location = 6) in vec3 aInstanceColor;
layout (location = 7) in float aInstanceScale;

// Standard uniforms
uniform mat4 view;
uniform mat4 projection;

// Outputs to fragment shader
out vec3 FragPos;
out vec3 Normal;
out vec3 InstanceColor;

void main() {
    // Apply instance scale to the position
    vec3 scaledPos = aPos * aInstanceScale;
    
    // Transform position using instance matrix
    vec4 worldPos = aInstanceMatrix * vec4(scaledPos, 1.0);
    FragPos = vec3(worldPos);
    
    // Transform normal using instance matrix (should be normalized)
    Normal = mat3(transpose(inverse(aInstanceMatrix))) * aNormal;
    
    // Pass instance color to fragment shader
    InstanceColor = aInstanceColor;
    
    // Final position
    gl_Position = projection * view * worldPos;
}