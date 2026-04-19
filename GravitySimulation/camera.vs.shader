#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in mat4 instanceModel;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool useInstancing;
uniform bool useGpuPositions;
uniform int instanceBaseIndex;

struct PhysicsBody {
	vec4 position;
	vec4 velocity;
	vec4 accumulated_force;
};

layout(std430, binding = 0) readonly buffer PhysicsData {
	PhysicsBody bodies[];
};

out vec3 FragPos;
out vec3 Normal;

void main() {
	mat4 finalModel = useInstancing ? instanceModel : model;
   if (useInstancing && useGpuPositions) {
		finalModel[3] = vec4(bodies[instanceBaseIndex + gl_InstanceID].position.xyz, 1.0);
	}
	vec4 worldPos = finalModel * vec4(aPos, 1.0);
	FragPos = vec3(worldPos);
    mat3 normalMatrix = transpose(inverse(mat3(finalModel)));
	Normal = normalMatrix * aNormal;

	gl_Position = projection * view * worldPos;
}