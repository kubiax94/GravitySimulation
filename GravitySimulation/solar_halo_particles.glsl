#version 460
layout(local_size_x = 64) in;

uniform float dt;
uniform float simulationTime;

struct PhysicsBody {
    vec4 position;
    vec4 velocity;
    vec4 accumulated_force;
};

layout(std430, binding = 0) buffer PhysicsData {
    PhysicsBody bodies[];
};

vec3 rotate_around_axis(vec3 v, vec3 axis, float angle) {
    vec3 nAxis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    return v * c + cross(nAxis, v) * s + nAxis * dot(nAxis, v) * (1.0 - c);
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= bodies.length())
        return;

    vec3 restPosition = bodies[i].accumulated_force.xyz;
    float restRadius = length(restPosition);
    if (restRadius <= 0.0001)
        return;

    vec3 radial = restPosition / restRadius;
    vec3 tangentSeed = bodies[i].velocity.xyz;
    float phase = bodies[i].velocity.w;
    float amplitude = bodies[i].accumulated_force.w;
    vec3 axisSeed = normalize(abs(radial.y) > 0.85 ? cross(radial, vec3(1.0, 0.0, 0.0)) : cross(radial, vec3(0.0, 1.0, 0.0)));
    vec3 swirlAxis = normalize(mix(axisSeed, tangentSeed, 0.55));

    float swirlA = simulationTime * (0.55 + amplitude * 7.0) + phase;
    float swirlB = simulationTime * (0.32 + amplitude * 5.0) - phase * 0.7;
    vec3 dir = rotate_around_axis(radial, swirlAxis, sin(swirlA) * amplitude * 2.4);
    dir = rotate_around_axis(dir, tangentSeed, cos(swirlB) * amplitude * 1.7);
    dir = normalize(dir);

    float pulse = 1.0 + sin(simulationTime * (1.8 + amplitude * 8.0) + phase * 1.3) * amplitude * 0.24;
    float radialOffset = sin(swirlA * 1.6) * amplitude * 0.58 + cos(swirlB * 1.9) * amplitude * 0.32;
    float radius = restRadius * pulse + radialOffset;

    bodies[i].position.xyz = dir * max(radius, 0.8);
    bodies[i].velocity.xyz = tangentSeed;
    bodies[i].velocity.w = phase;
    bodies[i].accumulated_force.xyz = restPosition;
}
