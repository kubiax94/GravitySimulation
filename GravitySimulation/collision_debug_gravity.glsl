#version 460
layout(local_size_x = 64) in;

uniform float rawDt;

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

    PhysicsBody body = bodies[i];
    float mass = body.position.w;
    if (mass <= 0.0) {
        body.velocity.xyz = vec3(0.0);
        body.accumulated_force.xyz = vec3(0.0);
        bodies[i] = body;
        return;
    }

    vec3 acceleration = body.accumulated_force.xyz / mass;
    acceleration += vec3(0.0, -9.81, 0.0);
    body.velocity.xyz += acceleration * rawDt;
    body.position.xyz += body.velocity.xyz * rawDt;
    body.accumulated_force.xyz = vec3(0.0);
    bodies[i] = body;
}
