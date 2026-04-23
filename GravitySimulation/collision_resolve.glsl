#version 460
layout(local_size_x = 64) in;

const uint collision_invalid_body_index = 0xffffffffu;
const uint collision_flag_enabled = 1u << 0;
const uint collision_flag_trigger = 1u << 1;
const uint collision_flag_dynamic = 1u << 2;
const uint collision_contact_flag_active = 1u << 0;
const uint collision_contact_flag_trigger = 1u << 1;

struct PhysicsBody {
    vec4 position;
    vec4 velocity;
    vec4 accumulated_force;
};

struct CollisionBody {
    vec4 center;
    vec4 half_extents;
    uvec4 metadata;
};

struct CollisionContact {
    uvec4 metadata;
    vec4 normal_penetration;
};

layout(std430, binding = 0) buffer PhysicsData {
    PhysicsBody bodies[];
};

layout(std430, binding = 1) buffer CollisionData {
    CollisionBody colliders[];
};

layout(std430, binding = 2) buffer CollisionContactData {
    CollisionContact contacts[];
};

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= contacts.length())
        return;

    CollisionContact contact = contacts[i];
    if ((contact.metadata.z & collision_contact_flag_active) == 0u)
        return;
    if ((contact.metadata.z & collision_contact_flag_trigger) != 0u)
        return;

    uint firstIndex = contact.metadata.x;
    uint secondIndex = contact.metadata.y;
    if (firstIndex == collision_invalid_body_index || secondIndex == collision_invalid_body_index)
        return;
    if (firstIndex >= bodies.length() || secondIndex >= bodies.length())
        return;

    PhysicsBody first = bodies[firstIndex];
    CollisionBody firstCollider = colliders[firstIndex];
    CollisionBody secondCollider = colliders[secondIndex];

    bool firstDynamic = (firstCollider.metadata.w & collision_flag_dynamic) != 0u && first.position.w > 0.0;
    bool secondDynamic = (secondCollider.metadata.w & collision_flag_dynamic) != 0u;
    if (!firstDynamic)
        return;

    vec3 normal = contact.normal_penetration.xyz;
    float penetration = contact.normal_penetration.w;
    vec3 separation = normal * penetration;
    vec3 firstDelta = secondDynamic ? (-separation * 0.5) : (-separation);

    first.position.xyz += firstDelta;
    float normalVelocity = dot(first.velocity.xyz, normal);
    if (normalVelocity < 0.0)
        first.velocity.xyz -= normal * normalVelocity;
    bodies[firstIndex] = first;
}
