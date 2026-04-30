#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 systemModel;
uniform mat4 view;
uniform mat4 projection;
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
uniform sampler2D waterAtlasTexture;
uniform int waterContinuityTextureAvailable;
uniform sampler2D waterContinuityTexture;
uniform int waterVetoTextureAvailable;
uniform sampler2D waterVetoTexture;
uniform int waterLevelTextureAvailable;
uniform sampler2D waterLevelTexture;
uniform int waveStateTextureAvailable;
uniform sampler2D waveStateTexture;
uniform int tidalHeightTextureAvailable;
uniform sampler2D tidalHeightTexture;
uniform int regionIdTextureAvailable;
uniform usampler2D regionIdTexture;
uniform int shoreDistanceTextureAvailable;
uniform sampler2D shoreDistanceTexture;

out vec3 FragPos;
out vec3 Normal;
out vec3 LocalSurfaceDir;
out vec2 AtlasUv;
out vec3 AtlasData;
out float AtlasFlood;
out float WaterColumnDepth01;
out float ShellSupport;
out float ShorelineFade;
out float BaseHydrologySupport;
out float WaterLevel01;
out float ContinuityCoverage;
out float WaterVeto;
out float TerrainOceanMask;
flat out uint WaterRegionId;
out float ShoreDistance;
out float WaveHeight;
out float WaveVelocity;
out float TidalHeight;

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

float earth_ocean_mask(vec3 n, float continents, float terrain) {
    float basinNoise = 0.5 + 0.5 * fbm(n.zxy * 2.1 + vec3(-0.7, 0.5, 1.2));
    float shelfNoise = 0.5 + 0.5 * fbm(n.yzx * 3.4 + vec3(1.1, -0.4, 0.2));
    float basinShape = smoothstep(0.24, 0.7, 1.0 - continents) * (0.55 + 0.45 * basinNoise);
    float seaFill = smoothstep(terrainSeaLevel + 0.02, terrainSeaLevel - 0.1, terrain);
    return clamp(seaFill * basinShape * (0.65 + 0.35 * shelfNoise), 0.0, 1.0);
}

float terrain_ocean_mask(vec3 n) {
    float continents = continent_mask(n);
    float terrain = terrain_height(n);
    float oceanFill = clamp((terrainSeaLevel - terrain) / 0.24, 0.0, 1.0);
    if (terrainEarthMacroContinentStrength > 0.001)
        oceanFill = max(oceanFill, earth_ocean_mask(n, continents, terrain));

    return smoothstep(0.02, 0.18, oceanFill);
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

void build_tangent_frame(vec3 normal, out vec3 tangent, out vec3 bitangent) {
    vec3 absNormal = abs(normal);
    vec3 helperAxis = absNormal.x <= absNormal.y && absNormal.x <= absNormal.z
        ? vec3(1.0, 0.0, 0.0)
        : (absNormal.y <= absNormal.z ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0));
    tangent = normalize(cross(helperAxis, normal));
    bitangent = normalize(cross(normal, tangent));
}

float approximate_basin_depth(vec3 n) {
    float centerFloor = terrain_surface_displacement(n);
    vec3 tangent;
    vec3 bitangent;
    build_tangent_frame(n, tangent, bitangent);

    const vec2 ringDirections[8] = vec2[8](
        vec2(1.0, 0.0),
        vec2(0.70710678, 0.70710678),
        vec2(0.0, 1.0),
        vec2(-0.70710678, 0.70710678),
        vec2(-1.0, 0.0),
        vec2(-0.70710678, -0.70710678),
        vec2(0.0, -1.0),
        vec2(0.70710678, -0.70710678));
    const float ringOffsets[3] = float[3](0.035, 0.07, 0.14);

    float barrierFloor = centerFloor;
    for (int ring = 0; ring < 3; ++ring) {
        float ringMin = 1e9;
        for (int dirIndex = 0; dirIndex < 8; ++dirIndex) {
            vec2 dir = ringDirections[dirIndex];
            vec3 sampleNormal = normalize(n + (tangent * dir.x + bitangent * dir.y) * ringOffsets[ring]);
            ringMin = min(ringMin, terrain_surface_displacement(sampleNormal));
        }

        barrierFloor = max(barrierFloor, ringMin);
    }

    return max(barrierFloor - centerFloor, 0.0);
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

vec4 sample_stabilized_atlas(vec2 uv) {
    vec2 texel = 1.0 / vec2(textureSize(waterAtlasTexture, 0));
    vec4 value = textureLod(waterAtlasTexture, uv, 0.0) * 0.24;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0)), 0.0) * 0.10;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0) * 0.10;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0) * 0.10;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0) * 0.10;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0) * 0.05;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0) * 0.05;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0) * 0.05;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0) * 0.05;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x * 2.0), clamp(uv.y, 0.0, 1.0)), 0.0) * 0.04;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x * 2.0 + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0) * 0.04;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y * 2.0, 0.0, 1.0)), 0.0) * 0.04;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y * 2.0, 0.0, 1.0)), 0.0) * 0.04;
    return value;
}

vec4 sample_polar_stable_atlas(vec2 uv, vec3 normal) {
    vec4 base = sample_stabilized_atlas(uv);
    float polarBlend = smoothstep(0.78, 0.98, abs(normal.y));
    if (polarBlend <= 0.0)
        return base;

    vec2 texel = 1.0 / vec2(textureSize(waterAtlasTexture, 0));
    vec4 filtered = textureLod(waterAtlasTexture, uv, 0.0) * 0.30;
    filtered += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x * 2.0), uv.y), 0.0) * 0.18;
    filtered += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x * 2.0 + 1.0), uv.y), 0.0) * 0.18;
    filtered += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x * 4.0), uv.y), 0.0) * 0.12;
    filtered += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x * 4.0 + 1.0), uv.y), 0.0) * 0.12;
    filtered += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x * 8.0), uv.y), 0.0) * 0.05;
    filtered += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x * 8.0 + 1.0), uv.y), 0.0) * 0.05;
    return mix(base, filtered, polarBlend);
}

vec4 decode_atlas_channels(vec4 atlasSample) {
    float atlasWeight = max(atlasSample.r, 0.0);
    if (atlasWeight <= 0.00001)
        return vec4(0.0);

    return vec4(
        clamp(atlasWeight, 0.0, 1.0),
        clamp(atlasSample.g / atlasWeight, 0.0, 1.0),
        clamp(atlasSample.b / atlasWeight, 0.0, 1.0),
        clamp(atlasSample.a / atlasWeight, 0.0, 1.0));
}

vec4 sample_continuous_atlas_data(vec2 uv, vec3 normal) {
    vec2 texel = 1.0 / vec2(textureSize(waterAtlasTexture, 0));
    vec4 center = decode_atlas_channels(sample_polar_stable_atlas(uv, normal));
    vec4 accumulated = center * 0.42;
    float weightSum = 0.42;

    const vec2 offsets[8] = vec2[8](
        vec2(1.0, 0.0),
        vec2(-1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, -1.0),
        vec2(2.0, 0.0),
        vec2(-2.0, 0.0),
        vec2(0.0, 2.0),
        vec2(0.0, -2.0));
    const float offsetWeights[8] = float[8](0.14, 0.14, 0.10, 0.10, 0.05, 0.05, 0.04, 0.04);

    for (int i = 0; i < 8; ++i) {
        vec2 sampleUv = vec2(
            fract(uv.x + offsets[i].x * texel.x + 1.0),
            clamp(uv.y + offsets[i].y * texel.y, 0.0, 1.0));
        vec4 sampleData = decode_atlas_channels(sample_polar_stable_atlas(sampleUv, normal));
        float similarity = 1.0 - smoothstep(0.16, 0.58,
            abs(sampleData.y - center.y) + abs(sampleData.w - center.w) + abs(sampleData.x - center.x) * 0.45);
        float weight = offsetWeights[i] * similarity;
        accumulated += sampleData * weight;
        weightSum += weight;
    }

    return weightSum > 0.0 ? accumulated / weightSum : center;
}

float sample_smoothed_water_level(vec2 uv, vec3 normal) {
    if (waterLevelTextureAvailable == 0)
        return 1.0;

    vec2 texel = 1.0 / vec2(textureSize(waterLevelTexture, 0));
    float value = textureLod(waterLevelTexture, uv, 0.0).r * 0.36;
    value += textureLod(waterLevelTexture, vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.12;
    value += textureLod(waterLevelTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.12;
    value += textureLod(waterLevelTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0).r * 0.12;
    value += textureLod(waterLevelTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0).r * 0.12;
    value += textureLod(waterLevelTexture, vec2(fract(uv.x + texel.x), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0).r * 0.04;
    value += textureLod(waterLevelTexture, vec2(fract(uv.x + texel.x), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0).r * 0.04;
    value += textureLod(waterLevelTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0).r * 0.04;
    value += textureLod(waterLevelTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0).r * 0.04;
    float base = clamp(value, 0.0, 1.0);
    float polarBlend = smoothstep(0.78, 0.98, abs(normal.y));
    if (polarBlend <= 0.0)
        return base;

    float filtered = textureLod(waterLevelTexture, uv, 0.0).r * 0.34;
    filtered += textureLod(waterLevelTexture, vec2(fract(uv.x + texel.x * 2.0), uv.y), 0.0).r * 0.18;
    filtered += textureLod(waterLevelTexture, vec2(fract(uv.x - texel.x * 2.0 + 1.0), uv.y), 0.0).r * 0.18;
    filtered += textureLod(waterLevelTexture, vec2(fract(uv.x + texel.x * 4.0), uv.y), 0.0).r * 0.12;
    filtered += textureLod(waterLevelTexture, vec2(fract(uv.x - texel.x * 4.0 + 1.0), uv.y), 0.0).r * 0.12;
    filtered += textureLod(waterLevelTexture, vec2(fract(uv.x + texel.x * 8.0), uv.y), 0.0).r * 0.03;
    filtered += textureLod(waterLevelTexture, vec2(fract(uv.x - texel.x * 8.0 + 1.0), uv.y), 0.0).r * 0.03;
    return mix(base, clamp(filtered, 0.0, 1.0), polarBlend);
}

float sample_water_veto(vec2 uv, vec3 normal) {
    if (waterVetoTextureAvailable == 0)
        return 0.0;

    vec2 texel = 1.0 / vec2(textureSize(waterVetoTexture, 0));
    float center = textureLod(waterVetoTexture, uv, 0.0).r;
    float value = center * 0.44;
    value += textureLod(waterVetoTexture, vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.14;
    value += textureLod(waterVetoTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.14;
    value += textureLod(waterVetoTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0).r * 0.08;
    value += textureLod(waterVetoTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0).r * 0.08;
    value += textureLod(waterVetoTexture, vec2(fract(uv.x + texel.x * 2.0), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.06;
    value += textureLod(waterVetoTexture, vec2(fract(uv.x - texel.x * 2.0 + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.06;

    float polarBlend = smoothstep(0.78, 0.98, abs(normal.y));
    float conservative = center * 0.56;
    conservative += textureLod(waterVetoTexture, vec2(fract(uv.x + texel.x), uv.y), 0.0).r * 0.11;
    conservative += textureLod(waterVetoTexture, vec2(fract(uv.x - texel.x + 1.0), uv.y), 0.0).r * 0.11;
    return mix(value, conservative, polarBlend);
}

float sample_smoothed_water_continuity(vec2 uv, vec3 normal) {
    if (waterContinuityTextureAvailable == 0)
        return 0.0;

    vec2 texel = 1.0 / vec2(textureSize(waterContinuityTexture, 0));
    float value = textureLod(waterContinuityTexture, uv, 0.0).r * 0.36;
    value += textureLod(waterContinuityTexture, vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.12;
    value += textureLod(waterContinuityTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.12;
    value += textureLod(waterContinuityTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0).r * 0.12;
    value += textureLod(waterContinuityTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0).r * 0.12;
    value += textureLod(waterContinuityTexture, vec2(fract(uv.x + texel.x), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0).r * 0.04;
    value += textureLod(waterContinuityTexture, vec2(fract(uv.x + texel.x), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0).r * 0.04;
    value += textureLod(waterContinuityTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0).r * 0.04;
    value += textureLod(waterContinuityTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0).r * 0.04;
    float base = clamp(value, 0.0, 1.0);
    float polarBlend = smoothstep(0.78, 0.98, abs(normal.y));
    if (polarBlend <= 0.0)
        return base;

    float filtered = textureLod(waterContinuityTexture, uv, 0.0).r * 0.34;
    filtered += textureLod(waterContinuityTexture, vec2(fract(uv.x + texel.x * 2.0), uv.y), 0.0).r * 0.18;
    filtered += textureLod(waterContinuityTexture, vec2(fract(uv.x - texel.x * 2.0 + 1.0), uv.y), 0.0).r * 0.18;
    filtered += textureLod(waterContinuityTexture, vec2(fract(uv.x + texel.x * 4.0), uv.y), 0.0).r * 0.12;
    filtered += textureLod(waterContinuityTexture, vec2(fract(uv.x - texel.x * 4.0 + 1.0), uv.y), 0.0).r * 0.12;
    filtered += textureLod(waterContinuityTexture, vec2(fract(uv.x + texel.x * 8.0), uv.y), 0.0).r * 0.03;
    filtered += textureLod(waterContinuityTexture, vec2(fract(uv.x - texel.x * 8.0 + 1.0), uv.y), 0.0).r * 0.03;
    return mix(base, clamp(filtered, 0.0, 1.0), polarBlend);
}

float sample_continuous_water_level(vec2 uv, vec3 normal) {
    if (waterLevelTextureAvailable == 0)
        return 1.0;

    vec2 texel = 1.0 / vec2(textureSize(waterLevelTexture, 0));
    float center = sample_smoothed_water_level(uv, normal);
    float accumulated = center * 0.42;
    float weightSum = 0.42;

    const vec2 offsets[8] = vec2[8](
        vec2(1.0, 0.0),
        vec2(-1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, -1.0),
        vec2(2.0, 0.0),
        vec2(-2.0, 0.0),
        vec2(0.0, 2.0),
        vec2(0.0, -2.0));
    const float offsetWeights[8] = float[8](0.14, 0.14, 0.10, 0.10, 0.05, 0.05, 0.04, 0.04);

    for (int i = 0; i < 8; ++i) {
        vec2 sampleUv = vec2(
            fract(uv.x + offsets[i].x * texel.x + 1.0),
            clamp(uv.y + offsets[i].y * texel.y, 0.0, 1.0));
        float sampleValue = sample_smoothed_water_level(sampleUv, normal);
        float similarity = 1.0 - smoothstep(0.10, 0.42, abs(sampleValue - center));
        float weight = offsetWeights[i] * similarity;
        accumulated += sampleValue * weight;
        weightSum += weight;
    }

    return clamp(weightSum > 0.0 ? accumulated / weightSum : center, 0.0, 1.0);
}

uint sample_region_id(vec2 uv) {
    if (regionIdTextureAvailable == 0)
        return 0u;

    ivec2 size = textureSize(regionIdTexture, 0);
    ivec2 coord = ivec2(
        clamp(int(floor(fract(uv.x + 1.0) * float(size.x))), 0, size.x - 1),
        clamp(int(floor(clamp(uv.y, 0.0, 1.0) * float(size.y))), 0, size.y - 1));
    return texelFetch(regionIdTexture, coord, 0).r;
}

float sample_shore_distance(vec2 uv) {
    if (shoreDistanceTextureAvailable == 0)
        return 0.0;

    return textureLod(shoreDistanceTexture, vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0).r;
}

vec2 sample_wave_state(vec2 uv) {
    if (waveStateTextureAvailable == 0)
        return vec2(0.0);

    return textureLod(waveStateTexture, vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0).rg;
}

vec2 sample_smoothed_wave_state(vec2 uv, vec3 normal) {
    if (waveStateTextureAvailable == 0)
        return vec2(0.0);

    vec2 texel = 1.0 / vec2(textureSize(waveStateTexture, 0));
    vec2 value = sample_wave_state(uv) * 0.36;
    value += sample_wave_state(vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0))) * 0.12;
    value += sample_wave_state(vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0))) * 0.12;
    value += sample_wave_state(vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.12;
    value += sample_wave_state(vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.12;
    value += sample_wave_state(vec2(fract(uv.x + texel.x), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.04;
    value += sample_wave_state(vec2(fract(uv.x + texel.x), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.04;
    value += sample_wave_state(vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.04;
    value += sample_wave_state(vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.04;

    float polarBlend = smoothstep(0.78, 0.98, abs(normal.y));
    if (polarBlend <= 0.0)
        return value;

    vec2 polarFiltered = sample_wave_state(uv) * 0.34;
    polarFiltered += sample_wave_state(vec2(fract(uv.x + texel.x * 2.0), uv.y)) * 0.18;
    polarFiltered += sample_wave_state(vec2(fract(uv.x - texel.x * 2.0 + 1.0), uv.y)) * 0.18;
    polarFiltered += sample_wave_state(vec2(fract(uv.x + texel.x * 4.0), uv.y)) * 0.12;
    polarFiltered += sample_wave_state(vec2(fract(uv.x - texel.x * 4.0 + 1.0), uv.y)) * 0.12;
    polarFiltered += sample_wave_state(vec2(fract(uv.x + texel.x * 8.0), uv.y)) * 0.03;
    polarFiltered += sample_wave_state(vec2(fract(uv.x - texel.x * 8.0 + 1.0), uv.y)) * 0.03;
    return mix(value, polarFiltered, polarBlend);
}

float sample_tidal_height(vec2 uv, vec3 normal) {
    if (tidalHeightTextureAvailable == 0)
        return 0.0;

    vec2 texel = 1.0 / vec2(textureSize(tidalHeightTexture, 0));
    float value = textureLod(tidalHeightTexture, uv, 0.0).r * 0.40;
    value += textureLod(tidalHeightTexture, vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.14;
    value += textureLod(tidalHeightTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.14;
    value += textureLod(tidalHeightTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0).r * 0.10;
    value += textureLod(tidalHeightTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0).r * 0.10;
    value += textureLod(tidalHeightTexture, vec2(fract(uv.x + texel.x * 2.0), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.06;
    value += textureLod(tidalHeightTexture, vec2(fract(uv.x - texel.x * 2.0 + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0).r * 0.06;
    float polarBlend = smoothstep(0.78, 0.98, abs(normal.y));
    float stretched = textureLod(tidalHeightTexture, uv, 0.0).r * 0.32;
    stretched += textureLod(tidalHeightTexture, vec2(fract(uv.x + texel.x * 4.0), uv.y), 0.0).r * 0.18;
    stretched += textureLod(tidalHeightTexture, vec2(fract(uv.x - texel.x * 4.0 + 1.0), uv.y), 0.0).r * 0.18;
    return mix(value, stretched, polarBlend);
}

void main() {
    vec3 localNormal = normalize(aPos);
    AtlasUv = build_planetary_hydrology_uv(localNormal);
    WaterRegionId = sample_region_id(AtlasUv);
    ShoreDistance = sample_shore_distance(AtlasUv);
    vec2 waveState = sample_smoothed_wave_state(AtlasUv, localNormal);
    WaveHeight = waveState.x;
    WaveVelocity = waveState.y;
    TidalHeight = sample_tidal_height(AtlasUv, localNormal);
    ContinuityCoverage = sample_smoothed_water_continuity(AtlasUv, localNormal);
    WaterVeto = sample_water_veto(AtlasUv, localNormal);

    vec4 atlasDataSample = sample_continuous_atlas_data(AtlasUv, localNormal);
    float occupancy = max(clamp(atlasDataSample.x, 0.0, 1.0), ContinuityCoverage * 0.92);
    float depth01 = clamp(atlasDataSample.y, 0.0, 1.0);
    float carrier = clamp(atlasDataSample.z, 0.0, 1.0);
    float flood = clamp(atlasDataSample.w, 0.0, 1.0);

    float terrainOceanMask = terrain_ocean_mask(localNormal);
    TerrainOceanMask = terrainOceanMask;
    float dynamicWaterLevel = waterLevelTextureAvailable != 0
        ? sample_continuous_water_level(AtlasUv, localNormal)
        : terrainOceanMask;
    float waterLevelPresence = waterLevelTextureAvailable != 0
        ? smoothstep(0.01, 0.08, dynamicWaterLevel)
        : 0.0;
    float blendedWaterLevel = waterLevelTextureAvailable != 0
        ? clamp(max(terrainOceanMask * 0.72, mix(terrainOceanMask, dynamicWaterLevel, waterLevelPresence)), 0.0, 1.0)
        : terrainOceanMask;
    float terrainOceanSupport = smoothstep(0.16, 0.52, terrainOceanMask);
    float particleOceanSupport = smoothstep(0.10, 0.34, occupancy) * smoothstep(0.08, 0.28, depth01) * smoothstep(0.18, 0.50, WaterVeto);
    float continuityLandGate = max(terrainOceanSupport, particleOceanSupport);
    float shorelineContinuity = smoothstep(0.08, 0.42, clamp(ShoreDistance / max(planetaryShellThickness * 0.30, 0.0001), 0.0, 1.0));
    float atlasContinuity = clamp(max(occupancy * (0.32 + depth01 * 0.48), flood * 0.78), 0.0, 1.0) * shorelineContinuity;
    blendedWaterLevel = clamp(max(blendedWaterLevel, max(atlasContinuity * 0.14, ContinuityCoverage * 0.10) * continuityLandGate * smoothstep(0.18, 0.48, WaterVeto)), 0.0, 1.0);

    float floorRadius = planetaryRadius + terrain_surface_displacement(localNormal);
    float globalCeilingRadius = min(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness);
    WaterLevel01 = blendedWaterLevel;
    float localCeilingRadius = clamp(mix(planetaryRadius, planetaryRadius + planetaryShellThickness, WaterLevel01), floorRadius, globalCeilingRadius);
    float availableDepth = max(localCeilingRadius - floorRadius, 0.0);
    WaterColumnDepth01 = smoothstep(planetaryShellThickness * 0.003, max(planetaryShellThickness * 0.055, 0.0001), availableDepth);
    ShorelineFade = smoothstep(planetaryShellThickness * 0.002, max(planetaryShellThickness * 0.060, 0.0001), availableDepth)
        * smoothstep(planetaryShellThickness * 0.006, planetaryShellThickness * 0.120, availableDepth);
    BaseHydrologySupport = clamp(
        WaterLevel01
        * smoothstep(0.16, 0.56, WaterColumnDepth01)
        * smoothstep(0.18, 0.68, ShorelineFade),
        0.0,
        1.0);
    float continuityShellBoost = ContinuityCoverage
        * smoothstep(0.14, 0.42, WaterLevel01)
        * smoothstep(0.10, 0.38, ShorelineFade)
        * continuityLandGate;
    float continuousShellSupport = clamp(
        max(WaterLevel01, continuityShellBoost * 0.28)
        * smoothstep(0.08, 0.34, WaterColumnDepth01)
        * smoothstep(0.06, 0.42, ShorelineFade),
        0.0,
        1.0);
    ShellSupport = continuousShellSupport;
    float shellSurfaceLevel = clamp(
        max(WaterLevel01 * smoothstep(0.04, 0.30, WaterColumnDepth01), continuousShellSupport),
        0.0,
        1.0);
    float deepWaterBlend = smoothstep(0.18, 0.72, WaterColumnDepth01);
    float minSurfaceLift = min(availableDepth, planetaryShellThickness * 0.0025);
    float compressedDepthLift = min(
        availableDepth * mix(0.045, 0.14, shellSurfaceLevel),
        planetaryShellThickness * mix(0.012, 0.032, deepWaterBlend));
    float shellHeightAboveFloor = mix(
        minSurfaceLift,
        max(minSurfaceLift, compressedDepthLift),
        deepWaterBlend);
    float waveShoreSupport = clamp(ShoreDistance / max(planetaryShellThickness * 0.28, 0.0001), 0.0, 1.0);
    float waveDisplacement = WaveHeight
        * planetaryShellThickness
        * (0.010 + 0.032 * deepWaterBlend)
        * waveShoreSupport
        * smoothstep(0.08, 0.40, WaterLevel01)
        * smoothstep(0.06, 0.34, WaterColumnDepth01);
    float tideDisplacement = TidalHeight
        * planetaryShellThickness
        * (0.05 + 0.08 * smoothstep(0.14, 0.62, WaterLevel01))
        * smoothstep(0.14, 0.34, ContinuityCoverage)
        * smoothstep(0.10, 0.42, ShorelineFade)
        * continuityLandGate;
    float shellRadius = clamp(
        floorRadius + shellHeightAboveFloor,
        floorRadius,
        localCeilingRadius);
    shellRadius = clamp(shellRadius + tideDisplacement + waveDisplacement, floorRadius, localCeilingRadius);

    vec3 localPosition = planetaryCenter + localNormal * shellRadius;
    vec4 worldPos = systemModel * vec4(localPosition, 1.0);
    FragPos = worldPos.xyz;
    Normal = normalize(mat3(systemModel) * localNormal);
    LocalSurfaceDir = localNormal;
    AtlasData = vec3(occupancy, depth01, carrier);
    AtlasFlood = flood;
    gl_Position = projection * view * worldPos;
}
