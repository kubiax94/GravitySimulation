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
uniform int planetaryRenderMaskTextureAvailable;
uniform sampler2D planetaryRenderMaskTexture;
uniform int planetaryWaterLevelTextureAvailable;
uniform sampler2D planetaryWaterLevelTexture;

out float AtlasWeight;
out float AtlasDepth;
out float AtlasCarrier;
out float AtlasFlood;
out float AtlasWaveForcing;
out float AtlasWaveCarrier;

struct FluidParticle {
    vec4 position;
    vec4 velocity;
    vec4 predicted_position;
    vec4 delta_position;
    vec4 solver_data;
    vec4 debug_data;
};

layout(std430, binding = 0) readonly buffer FluidParticles {
    FluidParticle particles[];
};

float wave_noise(vec3 p) {
    float n = 0.0;
    n += sin(p.x * 2.7 + p.y * 3.4 + p.z * 2.1);
    n += 0.5 * sin(-p.x * 5.8 + p.y * 4.9 + p.z * 6.2);
    n += 0.25 * sin(p.x * 10.7 - p.y * 9.1 + p.z * 7.5);
    return n / 1.75;
}

float fbm(vec3 p) {
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

float crater_mask(vec3 p) {
    float a = 0.5 + 0.5 * sin(p.x * 18.0 + p.y * 11.0 + p.z * 14.0);
    float b = 0.5 + 0.5 * sin(-p.x * 23.0 + p.y * 19.0 - p.z * 17.0);
    float c = 0.5 + 0.5 * sin(p.x * 29.0 - p.y * 27.0 + p.z * 21.0);
    return smoothstep(0.78, 0.97, a * b * c);
}

float continent_blob(vec3 n, vec3 center, float innerDot, float outerDot) {
    return smoothstep(innerDot, outerDot, dot(n, normalize(center)));
}

float earth_macro_continent_mask(vec3 n) {
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

float continent_mask(vec3 n) {
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

float terrain_height(vec3 n) {
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

float terrain_macro_height(vec3 n) {
    float continents = continent_mask(n);
    float largeScale = 0.5 + 0.5 * fbm(n * terrainLargeFrequency);
    float mediumScale = 0.5 + 0.5 * fbm(n.zxy * (terrainMediumFrequency * 0.72) + vec3(1.7, -2.1, 0.9));
    return largeScale * 0.72
        + mediumScale * 0.18
        + (continents - 0.46) * 0.26 * terrainContinentContrast;
}

float terrain_surface_displacement(vec3 n) {
    if (planetaryTerrainEnabled == 0)
        return 0.0;

    float macroHeight = terrain_macro_height(n);
    float fullHeight = terrain_height(n);
    float reliefStrength = max(terrainReliefStrength, 0.01);
    float landLift = max(macroHeight - terrainSeaLevel + 0.01, 0.0);
    float landRelief = landLift * (0.55 + 0.23 * reliefStrength);
    float oceanShelf = -max(terrainSeaLevel - macroHeight, 0.0) * 0.18;
    float mountainRelief = pow(max(fullHeight - terrainMountainSharpness, 0.0), 1.15) * 0.38 * reliefStrength;
    float displacement = (landRelief + oceanShelf + mountainRelief) * terrainDisplacementStrength;
    float maxUpwardDisplacement = terrainDisplacementStrength * (0.55 + 0.40 * reliefStrength);
    return clamp(displacement, -terrainDisplacementStrength * 0.08, maxUpwardDisplacement);
}

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

float sample_polar_stable_latlong_texture(sampler2D textureSampler, vec3 normal) {
    vec2 uv = build_planetary_hydrology_uv(normal);
    float base = textureLod(textureSampler, uv, 0.0).r;
    float polarBlend = smoothstep(0.80, 0.98, abs(normal.y));
    if (polarBlend <= 0.0)
        return base;

    vec2 texel = 1.0 / vec2(textureSize(textureSampler, 0));
    float filtered = base * 0.34;
    filtered += textureLod(textureSampler, vec2(fract(uv.x + texel.x * 2.0), uv.y), 0.0).r * 0.17;
    filtered += textureLod(textureSampler, vec2(fract(uv.x - texel.x * 2.0 + 1.0), uv.y), 0.0).r * 0.17;
    filtered += textureLod(textureSampler, vec2(fract(uv.x + texel.x * 4.0), uv.y), 0.0).r * 0.12;
    filtered += textureLod(textureSampler, vec2(fract(uv.x - texel.x * 4.0 + 1.0), uv.y), 0.0).r * 0.12;
    filtered += textureLod(textureSampler, vec2(fract(uv.x + texel.x * 8.0), uv.y), 0.0).r * 0.04;
    filtered += textureLod(textureSampler, vec2(fract(uv.x - texel.x * 8.0 + 1.0), uv.y), 0.0).r * 0.04;
    return mix(base, filtered, polarBlend);
}

float sample_planetary_render_mask(vec3 normal) {
    if (planetaryRenderMaskTextureAvailable == 0)
        return 1.0;

    return sample_polar_stable_latlong_texture(planetaryRenderMaskTexture, normal);
}

float sample_planetary_water_level(vec3 normal) {
    if (planetaryWaterLevelTextureAvailable == 0)
        return clamp((planetaryWaterSurfaceRadius - planetaryRadius) / max(planetaryShellThickness, 0.0001), 0.0, 1.0);

    return sample_polar_stable_latlong_texture(planetaryWaterLevelTexture, normal);
}

void main() {
    FluidParticle particle = particles[gl_InstanceID];
    vec3 localPosition = particle.position.xyz;
    vec3 radial = localPosition - planetaryCenter;
    float radialLength = length(radial);
    vec3 radialNormal = radialLength > 0.000001 ? radial / radialLength : vec3(0.0, 1.0, 0.0);
    vec2 uv = build_planetary_hydrology_uv(radialNormal);

    float floodMask = clamp(sample_planetary_render_mask(radialNormal), 0.0, 1.0);
    float sampledWaterLevel = clamp(sample_planetary_water_level(radialNormal), 0.0, 1.0);
    float waterLevelPresence = smoothstep(0.01, 0.08, sampledWaterLevel);
    float waterLevel = clamp(max(floodMask * 0.72, mix(floodMask, sampledWaterLevel, waterLevelPresence)), 0.0, 1.0);
    float floorRadius = planetaryRadius + terrain_surface_displacement(radialNormal);
    float globalCeilingRadius = min(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness);
    float localWaterSurfaceRadius = mix(planetaryRadius, planetaryRadius + planetaryShellThickness, waterLevel);
    float ceilingRadius = clamp(localWaterSurfaceRadius, floorRadius, globalCeilingRadius);
    float availableDepth = max(ceilingRadius - floorRadius, 0.0);
    float localDepth = max(ceilingRadius - radialLength, 0.0);
    float depth01 = availableDepth > 0.000001 ? clamp(localDepth / availableDepth, 0.0, 1.0) : 0.0;
    float columnDepth01 = smoothstep(particleRadius * 0.12, particleRadius * 2.8, availableDepth);
    float surfaceBand01 = availableDepth > 0.000001
        ? 1.0 - smoothstep(particleRadius * 0.04, max(particleRadius * 0.9, availableDepth * 0.12), localDepth)
        : 0.0;
    float upperColumnSupport = 1.0 - smoothstep(0.22, 0.86, depth01);
    float retainedSurfaceCarrier = max(surfaceBand01, upperColumnSupport * 0.78);
    float deepWaterFill = smoothstep(0.08, 0.72, columnDepth01);
    float polarBlend = smoothstep(0.72, 0.96, abs(radialNormal.y));
    float latitudeCos = sqrt(max(1.0 - radialNormal.y * radialNormal.y, 0.0));
    float polarWeightCompensation = mix(1.0, max(latitudeCos, 0.38), polarBlend);
    float polarCoverage = mix(1.0, min(1.0 / max(latitudeCos, 0.58), 1.32), polarBlend);
    vec3 tangentialVelocity = particle.velocity.xyz - radialNormal * dot(particle.velocity.xyz, radialNormal);
    float tangentialSpeed = length(tangentialVelocity);
    float combinedFlowStrength = length(particle.debug_data.xyz);
    float coriolisStrength = abs(particle.solver_data.w);
    float tidalStrength = abs(particle.debug_data.w);

    AtlasCarrier = clamp(
        (0.42 + 0.58 * floodMask)
        * (0.72 + 0.48 * columnDepth01)
        * (0.64 + 0.42 * max(retainedSurfaceCarrier, deepWaterFill * 0.65)),
        0.0,
        1.0) * polarWeightCompensation;
    AtlasWeight = AtlasCarrier * mix(1.18, 1.62, max(retainedSurfaceCarrier, deepWaterFill * 0.72));
    AtlasDepth = depth01;
    AtlasFlood = floodMask;
    AtlasWaveCarrier = clamp(
        tangentialSpeed * 0.10
        + combinedFlowStrength * 0.60
        + coriolisStrength * 0.12,
        0.0,
        1.0) * polarWeightCompensation;
    AtlasWaveForcing = clamp(
        (combinedFlowStrength * 0.78
            + tangentialSpeed * 0.10
            + coriolisStrength * 0.14
            + tidalStrength * 240000.0)
        * (0.28 + 0.72 * max(columnDepth01, retainedSurfaceCarrier))
        * (0.30 + 0.70 * floodMask),
        0.0,
        1.0) * polarWeightCompensation;

    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    float pointScale = max(atlasResolution.y / 256.0, 0.5);
    float coverageSupport = max(retainedSurfaceCarrier, deepWaterFill);
    float coverageSizeBoost = mix(1.08, 1.34, coverageSupport) * mix(1.0, 1.16, floodMask);
    gl_PointSize = clamp((3.4 + particleSize * 0.36 + AtlasCarrier * 5.2 + deepWaterFill * 3.1) * pointScale * polarCoverage * coverageSizeBoost, 3.5, 18.0);
}
