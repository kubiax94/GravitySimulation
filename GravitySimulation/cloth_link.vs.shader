#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 2) in mat4 instanceModel;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool useInstancing;
uniform bool useGpuPositions;

struct PhysicsBody {
    vec4 position;
    vec4 velocity;
    vec4 accumulated_force;
};

layout(std430, binding = 0) readonly buffer PhysicsData {
    PhysicsBody bodies[];
};

int decode_index(float encoded) {
    return max(int(round(encoded)) - 1, -1);
}

void main() {
    mat4 encodedModel = useInstancing ? instanceModel : model;
    vec3 worldPos = vec3(0.0);

    if (useGpuPositions) {
        const int startIndex = decode_index(encodedModel[0][0]);
        const int endIndex = decode_index(encodedModel[1][1]);
        if (startIndex >= 0 && endIndex >= 0) {
            const float t = clamp(-aPos.z, 0.0, 1.0);
            worldPos = mix(bodies[startIndex].position.xyz, bodies[endIndex].position.xyz, t);
        }
    }

    gl_Position = projection * view * vec4(worldPos, 1.0);
}
