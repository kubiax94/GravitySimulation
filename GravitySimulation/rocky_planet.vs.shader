#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in mat4 instanceModel;
layout (location = 6) in int instancePhysicsIndex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool useInstancing;
uniform bool useGpuPositions;
uniform int instanceBaseIndex;
uniform int physicsBodyIndex;
uniform float terrainSeaLevel;
uniform float terrainContinentFrequency;
uniform float terrainContinentWarpStrength;
uniform float terrainLargeFrequency;
uniform float terrainMediumFrequency;
uniform float terrainDetailFrequency;
uniform float terrainRidgeFrequency;
uniform float terrainCraterStrength;
uniform float terrainMountainSharpness;
uniform float terrainReliefStrength;
uniform float terrainDisplacementStrength;
uniform float terrainContinentContrast;
uniform float terrainEarthMacroContinentStrength;
uniform float terrainArchipelagoStrength;

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
out vec3 LocalNormal;
flat out vec3 WorldNormalBasisX;
flat out vec3 WorldNormalBasisY;
flat out vec3 WorldNormalBasisZ;

float wave_noise(vec3 p)
{
	float n = 0.0;
	n += sin(p.x * 2.7 + p.y * 3.4 + p.z * 2.1);
	n += 0.5 * sin(-p.x * 5.8 + p.y * 4.9 + p.z * 6.2);
	n += 0.25 * sin(p.x * 10.7 - p.y * 9.1 + p.z * 7.5);
	return n / 1.75;
}

float fbm(vec3 p)
{
	float value = 0.0;
	float amplitude = 0.55;
	float frequency = 1.0;

	for (int i = 0; i < 5; ++i) {
		value += amplitude * wave_noise(p * frequency);
		frequency *= 1.95;
		amplitude *= 0.5;
		p = p.yzx + vec3(0.37, -0.21, 0.43);
	}

	return value;
}

float crater_mask(vec3 p)
{
	float a = 0.5 + 0.5 * sin(p.x * 18.0 + p.y * 11.0 + p.z * 14.0);
	float b = 0.5 + 0.5 * sin(-p.x * 23.0 + p.y * 19.0 - p.z * 17.0);
	float c = 0.5 + 0.5 * sin(p.x * 29.0 - p.y * 27.0 + p.z * 21.0);
	return smoothstep(0.78, 0.97, a * b * c);
}

float continent_blob(vec3 n, vec3 center, float innerDot, float outerDot)
{
	return smoothstep(innerDot, outerDot, dot(n, normalize(center)));
}

float earth_macro_continent_mask(vec3 n)
{
	float afroEurasia = continent_blob(n, vec3(0.82, 0.18, -0.54), 0.44, 0.76);
	float americas = continent_blob(n, vec3(-0.78, 0.08, 0.34), 0.46, 0.78);
	float australasia = continent_blob(n, vec3(0.18, -0.42, 0.88), 0.58, 0.84);
	float polarLand = continent_blob(n, vec3(0.18, 0.86, 0.12), 0.72, 0.9) * 0.22;

	float macroLand = max(afroEurasia, americas);
	macroLand = max(macroLand, australasia * 0.82);
	macroLand = max(macroLand, polarLand);

	float islandNoise = 0.5 + 0.5 * fbm(n.zxy * 5.6 + vec3(0.9, -1.4, 0.3));
	float coastalBand = smoothstep(0.22, 0.58, macroLand) * (1.0 - smoothstep(0.62, 0.9, macroLand));
	float archipelagos = coastalBand * smoothstep(0.56, 0.82, islandNoise) * terrainArchipelagoStrength;

	return clamp(max(macroLand, archipelagos), 0.0, 1.0);
}

float continent_mask(vec3 n)
{
	vec3 warped = normalize(n + vec3(
		fbm(n * (terrainContinentFrequency * 1.2) + vec3(0.7, -1.1, 0.4)),
		fbm(n.zxy * (terrainContinentFrequency * 1.35) + vec3(-0.3, 0.6, -0.8)),
		fbm(n.yzx * (terrainContinentFrequency * 1.5) + vec3(1.0, -0.2, 0.9)))
		* terrainContinentWarpStrength);
	float primary = 0.5 + 0.5 * fbm(warped * terrainContinentFrequency + vec3(1.3, -0.9, 0.6));
	float secondary = 0.5 + 0.5 * fbm(warped.zxy * (terrainContinentFrequency * 2.05) + vec3(-1.2, 0.4, 1.1));
	float tertiary = 0.5 + 0.5 * fbm(warped.yzx * (terrainContinentFrequency * 3.1) + vec3(0.5, 1.0, -0.7));
	float combined = primary * 0.68 + secondary * 0.24 + tertiary * 0.08;
    float genericMask = smoothstep(0.44, 0.62, combined);
	if (terrainEarthMacroContinentStrength <= 0.001)
		return genericMask;

	float macroMask = earth_macro_continent_mask(n);
	return clamp(mix(genericMask, max(genericMask * 0.4, macroMask), terrainEarthMacroContinentStrength), 0.0, 1.0);
}

float terrain_height(vec3 n)
{
	float continents = continent_mask(n);
	float largeScale = 0.5 + 0.5 * fbm(n * terrainLargeFrequency);
	float mediumScale = 0.5 + 0.5 * fbm(n.zxy * terrainMediumFrequency + vec3(1.7, -2.1, 0.9));
	float detailScale = 0.5 + 0.5 * fbm(n.yzx * terrainDetailFrequency + vec3(-3.2, 1.4, 2.6));
	float ridgeMask = pow(1.0 - abs(fbm(n * terrainRidgeFrequency + vec3(0.4, -0.8, 1.1))), 2.3);
	float craters = crater_mask(n * 1.3 + vec3(0.4, -0.6, 1.2));

	float height = largeScale * 0.55
		+ mediumScale * 0.27
		+ detailScale * 0.12
		+ ridgeMask * 0.06
		+ (continents - 0.46) * 0.22 * terrainContinentContrast;

	return height - craters * terrainCraterStrength;
}

float terrain_macro_height(vec3 n)
{
	float continents = continent_mask(n);
	float largeScale = 0.5 + 0.5 * fbm(n * terrainLargeFrequency);
	float mediumScale = 0.5 + 0.5 * fbm(n.zxy * (terrainMediumFrequency * 0.72) + vec3(1.7, -2.1, 0.9));
	float height = largeScale * 0.72
		+ mediumScale * 0.18
		+ (continents - 0.46) * 0.26 * terrainContinentContrast;

	return height;
}

void main() {
	mat4 finalModel = useInstancing ? instanceModel : model;
	if (useGpuPositions) {
		const int batchedPhysicsIndex = useInstancing && instanceBaseIndex >= 0 ? instanceBaseIndex + gl_InstanceID : -1;
		const int physicsIndex = useInstancing
			? (instancePhysicsIndex >= 0 ? instancePhysicsIndex : batchedPhysicsIndex)
			: physicsBodyIndex;
		if (physicsIndex >= 0)
			finalModel[3] = vec4(bodies[physicsIndex].position.xyz, 1.0);
	}

	vec3 localNormal = normalize(aNormal);
 float macroHeight = terrain_macro_height(localNormal);
	float fullHeight = terrain_height(localNormal);
    float reliefStrength = max(terrainReliefStrength, 0.01);
	float landLift = max(macroHeight - terrainSeaLevel + 0.01, 0.0);
 float landRelief = landLift * (0.55 + 0.23 * reliefStrength);
	float oceanShelf = -max(terrainSeaLevel - macroHeight, 0.0) * 0.18;
    float mountainRelief = pow(max(fullHeight - terrainMountainSharpness, 0.0), 1.15) * 0.38 * reliefStrength;
	float displacement = (landRelief + oceanShelf + mountainRelief) * terrainDisplacementStrength;
	float maxUpwardDisplacement = terrainDisplacementStrength * (0.55 + 0.40 * reliefStrength);
	displacement = clamp(displacement, -terrainDisplacementStrength * 0.08, maxUpwardDisplacement);
	vec3 displacedLocalPos = aPos + localNormal * displacement;

	vec4 worldPos = finalModel * vec4(displacedLocalPos, 1.0);
	FragPos = vec3(worldPos);
	mat3 normalMatrix = transpose(inverse(mat3(finalModel)));
	Normal = normalize(normalMatrix * localNormal);
	LocalNormal = localNormal;
	WorldNormalBasisX = normalMatrix[0];
	WorldNormalBasisY = normalMatrix[1];
	WorldNormalBasisZ = normalMatrix[2];

	gl_Position = projection * view * worldPos;
}
