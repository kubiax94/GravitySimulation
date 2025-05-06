#version 460
layout(local_size_x = 64) in;

uniform float G;

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

    vec3 pos_i = bodies[i].position.xyz;
    float mass_i = bodies[i].position.w;

    vec3 force = vec3(0);

    for(int j = 0; j < bodies.length(); j++) {
        
        if (i <= j) continue;
        
        vec3 pos_j = bodies[j].position.xyz;
        float mass_j = bodies[j].position.w;

        vec3 dir = pos_j - pos_i;
        float dist = length(dir);
        if(dist < 1e-3) continue;

        vec3 dir_norm = normalize(dir);
        float force_mag = G * mass_i * mass_j / (dist * dist);
        force += dir_norm * force_mag;
    }

    bodies[i].accumulated_force.xyz = force;
}