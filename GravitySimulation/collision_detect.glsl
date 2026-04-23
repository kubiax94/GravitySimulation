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
    uvec4 metadata[4];
    vec4 normal_penetration[4];
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

bool masks_match(uint lhsLayer, uint lhsQuery, uint rhsLayer, uint rhsQuery) {
    return (lhsLayer & rhsQuery) != 0u && (rhsLayer & lhsQuery) != 0u;
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= contacts.length() || i >= colliders.length())
        return;

    CollisionContact result;
    for (uint slot = 0u; slot < 4u; ++slot) {
        result.metadata[slot] = uvec4(collision_invalid_body_index, collision_invalid_body_index, 0u, 0u);
        result.normal_penetration[slot] = vec4(0.0);
    }

    CollisionBody first = colliders[i];
    if ((first.metadata.w & collision_flag_enabled) == 0u || first.metadata.x == collision_invalid_body_index) {
        contacts[i] = result;
        return;
    }

    int contactCount = 0;

    for (uint j = 0u; j < colliders.length(); ++j) {
        if (i == j)
            continue;

        CollisionBody second = colliders[j];
        if ((second.metadata.w & collision_flag_enabled) == 0u || second.metadata.x == collision_invalid_body_index)
            continue;

        if (!masks_match(first.metadata.y, first.metadata.z, second.metadata.y, second.metadata.z))
            continue;

        vec3 delta = second.center.xyz - first.center.xyz;
        vec3 overlap = (first.half_extents.xyz + second.half_extents.xyz) - abs(delta);
        if (overlap.x < 0.0 || overlap.y < 0.0 || overlap.z < 0.0)
            continue;

        float penetration = overlap.x;
        vec3 normal = vec3(delta.x >= 0.0 ? 1.0 : -1.0, 0.0, 0.0);
        if (overlap.y < penetration) {
            penetration = overlap.y;
            normal = vec3(0.0, delta.y >= 0.0 ? 1.0 : -1.0, 0.0);
        }
        if (overlap.z < penetration) {
            penetration = overlap.z;
            normal = vec3(0.0, 0.0, delta.z >= 0.0 ? 1.0 : -1.0);
        }

        if (contactCount >= 4)
            continue;

        uint flags = collision_contact_flag_active;
        if (((first.metadata.w | second.metadata.w) & collision_flag_trigger) != 0u)
            flags |= collision_contact_flag_trigger;

        int insertIndex = contactCount;
        float insertPenetration = penetration;
        for (int existing = 0; existing < contactCount; ++existing) {
            float existingPenetration = result.normal_penetration[existing].w;
            if (insertPenetration > existingPenetration) {
                insertIndex = existing;
                break;
            }
        }

        for (int shift = min(contactCount, 3); shift > insertIndex; --shift) {
            result.metadata[shift] = result.metadata[shift - 1];
            result.normal_penetration[shift] = result.normal_penetration[shift - 1];
        }

        result.metadata[insertIndex] = uvec4(first.metadata.x, j, flags, 0u);
        result.normal_penetration[insertIndex] = vec4(normal, penetration);
        contactCount = min(contactCount + 1, 4);
    }

    contacts[i] = result;
}
