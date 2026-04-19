#version 460 core

layout(location = 0) in vec3 aPos;

uniform mat4 systemModel;
uniform mat4 view;
uniform mat4 projection;
uniform float particleSize;

struct FluidParticle {
    vec4 position;
    vec4 velocity;
    vec4 predicted_position;
    vec4 delta_position;
    vec4 solver_data;
};

layout(std430, binding = 0) readonly buffer FluidParticles {
    FluidParticle particles[];
};

void main() {
    vec4 worldPos = systemModel * vec4(particles[gl_InstanceID].position.xyz + aPos, 1.0);
    gl_Position = projection * view * worldPos;
    gl_PointSize = particleSize;
}
