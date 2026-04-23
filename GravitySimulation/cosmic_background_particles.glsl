#version 460
layout(local_size_x = 64) in;

uniform float dt;
uniform float rawDt;
uniform float simulationTime;

struct PhysicsBody {
    vec4 position;
    vec4 velocity;
    vec4 accumulated_force;
};

layout(std430, binding = 0) buffer PhysicsData {
    PhysicsBody bodies[];
};

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= bodies.length())
        return;

    vec3 position = bodies[i].position.xyz;
    float radius = length(position);
    if (radius <= 0.0001)
        return;

    vec3 direction = position / radius;
    float phase = simulationTime * 0.002 + float(i) * 0.013;
    float drift = sin(phase) * 0.00012 + cos(phase * 0.73) * 0.00008;
    bodies[i].position.xyz = direction * (radius * (1.0 + drift));
    bodies[i].velocity.xyz = vec3(0.0);
    bodies[i].accumulated_force.xyz = vec3(0.0);
}
