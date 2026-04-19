#version 460
layout(local_size_x = 64) in;

uniform float G;
uniform float dt;

const float softening = 25.0;

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
    if (i >= bodies.length()) return;

    if (i == 0u) {
        bodies[i].velocity.xyz = vec3(0.0);
        bodies[i].accumulated_force.xyz = vec3(0.0);
        return;
    }

    vec3 pos = bodies[i].position.xyz;
    vec3 vel = bodies[i].velocity.xyz;
    vec3 corePos = bodies[0].position.xyz;
    float coreMass = bodies[0].position.w;

    vec3 dir = corePos - pos;
    float distSq = dot(dir, dir) + softening * softening;
    float invDist = inversesqrt(distSq);
    float invDist3 = invDist * invDist * invDist;
    vec3 acceleration = G * coreMass * dir * invDist3;

    vel += acceleration * dt;
    pos += vel * dt;

    bodies[i].velocity.xyz = vel;
    bodies[i].position.xyz = pos;
    bodies[i].accumulated_force.xyz = vec3(0);
}