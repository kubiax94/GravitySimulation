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
uniform int waterLevelTextureAvailable;
uniform sampler2D waterLevelTexture;

out vec3 FragPos;
out vec3 Normal;
out vec2 AtlasUv;
out vec3 AtlasData;
out float AtlasFlood;
out float WaterColumnDepth01;
out float ShellSupport;
out float ShorelineFade;
out float BaseHydrologySupport;
out float WaterLevel01;

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
    vec3 helperAxis = abs(normal.y) > 0.82 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
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
    vec4 value = textureLod(waterAtlasTexture, uv, 0.0) * 0.28;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0)), 0.0) * 0.12;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.0) * 0.12;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0) * 0.12;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0) * 0.12;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0) * 0.06;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0) * 0.06;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.0) * 0.06;
    value += textureLod(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.0) * 0.06;
    return value;
}

void main() {
    vec3 localNormal = normalize(aPos);
    AtlasUv = build_planetary_hydrology_uv(localNormal);

    vec4 atlasSample = sample_stabilized_atlas(AtlasUv);
    float atlasWeight = max(atlasSample.r, 0.0);
    float occupancy = smoothstep(0.015, 0.11, atlasWeight);
    float depth01 = atlasWeight > 0.00001 ? clamp(atlasSample.g / atlasWeight, 0.0, 1.0) : 0.0;
    float carrier = atlasWeight > 0.00001 ? clamp(atlasSample.b / atlasWeight, 0.0, 1.0) : 0.0;
    float flood = atlasWeight > 0.00001 ? clamp(atlasSample.a / atlasWeight, 0.0, 1.0) : 0.0;

    float floorRadius = planetaryRadius + terrain_surface_displacement(localNormal);
    float globalCeilingRadius = min(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness);
    float availableDepth = max(globalCeilingRadius - floorRadius, 0.0);
    float terrainOceanMask = terrain_ocean_mask(localNormal);
    WaterLevel01 = terrainOceanMask;
    WaterColumnDepth01 = smoothstep(planetaryShellThickness * 0.003, max(planetaryShellThickness * 0.055, 0.0001), availableDepth);
    ShorelineFade = smoothstep(planetaryShellThickness * 0.004, max(planetaryShellThickness * 0.045, 0.0001), availableDepth)
        * smoothstep(planetaryShellThickness * 0.010, planetaryShellThickness * 0.080, availableDepth);
    BaseHydrologySupport = clamp(
        terrainOceanMask
        * smoothstep(0.28, 0.68, WaterColumnDepth01)
        * smoothstep(0.38, 0.82, ShorelineFade),
        0.0,
        1.0);
    ShellSupport = occupancy * (0.30 + carrier * 0.70) * (0.42 + flood * 0.58) * max(WaterColumnDepth01, 0.52) * max(ShorelineFade, 0.62);
    float atlasSurfaceSupport = clamp(terrainOceanMask * (occupancy * 0.34 + carrier * 0.38 + flood * 0.28), 0.0, 1.0);
    float shellInset = mix(planetaryShellThickness * 0.24, planetaryShellThickness * 0.40, 1.0 - atlasSurfaceSupport);
    shellInset += (1.0 - WaterColumnDepth01) * planetaryShellThickness * 0.16;
    shellInset += (1.0 - ShorelineFade) * planetaryShellThickness * 0.26;
    shellInset += (1.0 - terrainOceanMask) * planetaryShellThickness * 0.30;
    float shellUpperRadius = clamp(globalCeilingRadius - shellInset, floorRadius, globalCeilingRadius);
    float shellLowerRadius = floorRadius + availableDepth * mix(0.16, 0.28, atlasSurfaceSupport);
    float shellRadius = clamp(mix(shellLowerRadius, shellUpperRadius, 0.42), floorRadius, globalCeilingRadius);

    vec3 localPosition = planetaryCenter + localNormal * shellRadius;
    vec4 worldPos = systemModel * vec4(localPosition, 1.0);
    FragPos = worldPos.xyz;
    Normal = normalize(mat3(systemModel) * localNormal);
    AtlasData = vec3(occupancy, depth01, carrier);
    AtlasFlood = flood;
    gl_Position = projection * view * worldPos;
}
