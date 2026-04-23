#version 460 core

layout(location = 0) in vec3 aPos;

uniform mat4 systemModel;
uniform mat4 view;
uniform mat4 projection;
uniform float particleSize;
uniform float particleSizeJitter;

struct PhysicsBody {
    vec4 position;
    vec4 velocity;
    vec4 accumulated_force;
};

layout(std430, binding = 0) readonly buffer PhysicsData {
    PhysicsBody bodies[];
};

flat out float ParticleSeed;

float hash11(float p) {
    return fract(sin(p * 127.1) * 43758.5453123);
}

void main() {
    vec4 worldPos = systemModel * vec4(bodies[gl_InstanceID].position.xyz + aPos, 1.0);
    gl_Position = projection * view * worldPos;
    ParticleSeed = hash11(float(gl_InstanceID) + 1.0);
    float sizeScale = mix(1.0, mix(0.58, 1.85, ParticleSeed), clamp(particleSizeJitter, 0.0, 1.0));
    gl_PointSize = particleSize * sizeScale;
}
