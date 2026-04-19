#version 460
layout(local_size_x = 64) in;

uniform float dt;
uniform float rawDt;
uniform int constraintCount;
uniform float gravityAcceleration;
uniform float springDamping;
uniform float velocityDamping;
uniform float floorHeight;
uniform float floorBounce;
uniform float tangentialDamping;
uniform float simulationTime;
uniform vec3 windDirection;
uniform float windStrength;
uniform float windPulseStrength;
uniform float windPulseFrequency;
uniform float windTurbulence;

struct PhysicsBody {
    vec4 position;
    vec4 velocity;
    vec4 accumulated_force;
};

struct ClothConstraint {
    uvec2 particle_indices;
    float rest_length;
    float stiffness;
};

layout(std430, binding = 0) buffer PhysicsData {
    PhysicsBody bodies[];
};

layout(std430, binding = 1) readonly buffer ClothConstraints {
    ClothConstraint constraints[];
};

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= bodies.length())
        return;

    float mass = bodies[i].position.w;
    if (mass <= 0.0) {
        bodies[i].velocity.xyz = vec3(0.0);
        return;
    }

    vec3 position = bodies[i].position.xyz;
    vec3 velocity = bodies[i].velocity.xyz;
    vec3 force = vec3(0.0, -gravityAcceleration * mass, 0.0);

    vec3 wind_dir = normalize(windDirection);
    vec3 wind_side = normalize(vec3(-wind_dir.z, 0.0, wind_dir.x) + vec3(0.0001, 0.0, 0.0));
    float pulse = 0.5 + 0.5 * sin(simulationTime * windPulseFrequency + position.x * 0.045 + position.y * 0.025);
    float turbulence = sin(simulationTime * 1.9 + position.x * 0.08 + float(i) * 0.17)
        + cos(simulationTime * 1.3 + position.y * 0.06 + float(i) * 0.11);
    vec3 wind_force = wind_dir * (windStrength + windPulseStrength * pulse)
        + wind_side * (windTurbulence * turbulence)
        + vec3(0.0, 6.0 * pulse + 2.5 * turbulence, 0.0);
    wind_force -= velocity * 0.18;
    force += wind_force;

    for (int constraint_index = 0; constraint_index < constraintCount; ++constraint_index) {
        ClothConstraint constraint = constraints[constraint_index];
        uint a = constraint.particle_indices.x;
        uint b = constraint.particle_indices.y;

        if (a != i && b != i)
            continue;

        uint other_index = (a == i) ? b : a;
        if (other_index >= bodies.length())
            continue;

        vec3 other_position = bodies[other_index].position.xyz;
        vec3 other_velocity = bodies[other_index].velocity.xyz;
        vec3 delta = other_position - position;
        float distance_value = length(delta);
        if (distance_value <= 0.0001)
            continue;

        vec3 direction = delta / distance_value;
        float stretch = distance_value - constraint.rest_length;
        float relative_velocity = dot(other_velocity - velocity, direction);
        float spring_force = constraint.stiffness * stretch + springDamping * relative_velocity;
        force += direction * spring_force;
    }

    vec3 acceleration = force / mass;
    velocity = (velocity + acceleration * rawDt) * velocityDamping;
    position += velocity * rawDt;

    if (position.y < floorHeight) {
        position.y = floorHeight;
        if (velocity.y < 0.0)
            velocity.y = -velocity.y * floorBounce;

        velocity.x *= tangentialDamping;
        velocity.z *= tangentialDamping;
    }

    bodies[i].velocity.xyz = velocity;
    bodies[i].position.xyz = position;
    bodies[i].accumulated_force.xyz = force;
}
