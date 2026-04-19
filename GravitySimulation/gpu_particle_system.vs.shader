#version 460 core

layout(location = 0) in vec3 aPos;

uniform mat4 systemModel;
uniform mat4 view;
uniform mat4 projection;
uniform float particleSize;

struct PhysicsBody {
    vec4 position;
    vec4 velocity;
    vec4 accumulated_force;
};

layout(std430, binding = 0) readonly buffer PhysicsData {
    PhysicsBody bodies[];
};

void main() {
    vec4 worldPos = systemModel * vec4(bodies[gl_InstanceID].position.xyz + aPos, 1.0);
    gl_Position = projection * view * worldPos;
    gl_PointSize = particleSize;
}
