#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in mat4 instanceModel;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool useInstancing;

out vec3 FragPos;
out vec3 Normal;

void main() {
	mat4 finalModel = useInstancing ? instanceModel : model;
	vec4 worldPos = finalModel * vec4(aPos, 1.0);
	FragPos = vec3(worldPos);
	Normal = mat3(transpose(inverse(finalModel))) * aNormal;

	gl_Position = projection * view * worldPos;
}