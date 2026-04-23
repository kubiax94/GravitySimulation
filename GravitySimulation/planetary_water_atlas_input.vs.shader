#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform vec2 atlasResolution;
uniform float particleRadius;
uniform float particleSize;
uniform vec3 planetaryCenter;
uniform float planetaryRadius;
uniform float planetaryShellThickness;
uniform float planetaryWaterSurfaceRadius;
uniform int planetaryTerrainEnabled;
uniform int planetaryRenderMaskTextureAvailable;
uniform sampler2D planetaryRenderMaskTexture;
uniform int planetaryWaterLevelTextureAvailable;
uniform sampler2D planetaryWaterLevelTexture;

out float AtlasWeight;
out float AtlasDepth;
out float AtlasCarrier;
out float AtlasFlood;

struct FluidParticle {
    vec4 position;
    vec4 velocity;
    vec4 predicted_position;
    vec4 delta_position;
    vec4 solver_data;
};

layout(std430, binding = 0) readonly buffer FluidParticles {
    FluidParticle particles[];
};

vec2 build_planetary_hydrology_uv(vec3 normal) {
    vec3 safeNormal = dot(normal, normal) > 0.000001
        ? normalize(normal)
        : vec3(0.0, 1.0, 0.0);
    float latitude = asin(clamp(safeNormal.y, -1.0, 1.0));
    float longitude = atan(safeNormal.z, safeNormal.x);
    return vec2(
        (longitude + 3.14159265359) / 6.28318530718,
        (latitude + 1.57079632679) / 3.14159265359);
}

float sample_planetary_render_mask(vec3 normal) {
    if (planetaryRenderMaskTextureAvailable == 0)
        return 1.0;

    return textureLod(planetaryRenderMaskTexture, build_planetary_hydrology_uv(normal), 0.0).r;
}

float sample_planetary_water_level(vec3 normal) {
    if (planetaryWaterLevelTextureAvailable == 0)
        return clamp((planetaryWaterSurfaceRadius - planetaryRadius) / max(planetaryShellThickness, 0.0001), 0.0, 1.0);

    return textureLod(planetaryWaterLevelTexture, build_planetary_hydrology_uv(normal), 0.0).r;
}

void main() {
    FluidParticle particle = particles[gl_InstanceID];
    vec3 localPosition = particle.position.xyz;
    vec3 radial = localPosition - planetaryCenter;
    float radialLength = length(radial);
    vec3 radialNormal = radialLength > 0.000001 ? radial / radialLength : vec3(0.0, 1.0, 0.0);
    vec2 uv = build_planetary_hydrology_uv(radialNormal);

    float floodMask = clamp(sample_planetary_render_mask(radialNormal), 0.0, 1.0);
    float waterLevel = clamp(sample_planetary_water_level(radialNormal), 0.0, 1.0);
    float floorRadius = planetaryRadius;
    float globalCeilingRadius = min(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness);
    float ceilingRadius = clamp(mix(planetaryRadius, planetaryRadius + planetaryShellThickness, waterLevel), floorRadius, globalCeilingRadius);
    float availableDepth = max(ceilingRadius - floorRadius, 0.0);
    float localDepth = max(ceilingRadius - radialLength, 0.0);
    float depth01 = availableDepth > 0.000001 ? clamp(localDepth / availableDepth, 0.0, 1.0) : 0.0;
    float columnDepth01 = smoothstep(particleRadius * 0.12, particleRadius * 2.8, availableDepth);
    float surfaceBand01 = availableDepth > 0.000001
        ? 1.0 - smoothstep(particleRadius * 0.04, max(particleRadius * 0.9, availableDepth * 0.12), localDepth)
        : 0.0;
    float upperColumnSupport = 1.0 - smoothstep(0.22, 0.86, depth01);
    float retainedSurfaceCarrier = max(surfaceBand01, upperColumnSupport * 0.78);

    AtlasCarrier = clamp(
        (0.42 + 0.58 * floodMask)
        * (0.62 + 0.38 * columnDepth01)
        * (0.58 + 0.42 * retainedSurfaceCarrier),
        0.0,
        1.0);
    AtlasWeight = AtlasCarrier * mix(0.88, 1.20, retainedSurfaceCarrier);
    AtlasDepth = depth01;
    AtlasFlood = floodMask;

    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    float pointScale = max(atlasResolution.y / 256.0, 0.5);
    gl_PointSize = clamp((1.8 + particleSize * 0.22 + AtlasCarrier * 2.8) * pointScale, 1.5, 9.0);
}
