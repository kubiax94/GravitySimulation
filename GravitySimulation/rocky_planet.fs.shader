#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec3 LocalNormal;
flat in vec3 WorldNormalBasisX;
flat in vec3 WorldNormalBasisY;
flat in vec3 WorldNormalBasisZ;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;
uniform float terrainSeaLevel;
uniform float terrainContinentFrequency;
uniform float terrainContinentWarpStrength;
uniform float terrainBiomeFrequency;
uniform float terrainLargeFrequency;
uniform float terrainMediumFrequency;
uniform float terrainDetailFrequency;
uniform float terrainRidgeFrequency;
uniform float terrainCraterStrength;
uniform float terrainMountainSharpness;
uniform float terrainReliefStrength;
uniform float terrainDisplacementStrength;
uniform vec3 terrainRockDarkColor;
uniform vec3 terrainRockMidColor;
uniform vec3 terrainRockBrightColor;
uniform vec3 terrainDustColor;
uniform vec3 terrainIceColor;
uniform vec3 terrainShallowOceanColor;
uniform vec3 terrainDeepOceanColor;
uniform vec3 terrainVegetationColor;
uniform vec3 terrainCoastColor;
uniform vec3 terrainInteriorColor;
uniform vec3 terrainMountainColor;
uniform float terrainOceanVisibility;
uniform int terrainStaticOceanTintEnabled;
uniform float terrainVegetationStrength;
uniform float terrainContinentContrast;
uniform float terrainEarthMacroContinentStrength;
uniform float terrainArchipelagoStrength;
uniform int terrainDebugMode;
const int floodDebugPointCapacity = 256;
uniform int floodDebugPointCount;
uniform vec3 floodDebugNormals[floodDebugPointCapacity];
uniform int floodDebugRegionIndices[floodDebugPointCapacity];

float saturate(float v)
{
    return clamp(v, 0.0, 1.0);
}

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

float biome_mask(vec3 n)
{
    vec3 warped = normalize(n + vec3(
        fbm(n * (terrainBiomeFrequency * 0.9) + vec3(-0.8, 0.4, 0.7)),
        fbm(n.zxy * (terrainBiomeFrequency * 1.15) + vec3(0.5, -1.1, 0.3)),
        fbm(n.yzx * (terrainBiomeFrequency * 1.35) + vec3(1.1, 0.2, -0.9)))
        * 0.18 * max(terrainContinentWarpStrength, 0.15));
    float humidity = 0.5 + 0.5 * fbm(warped * terrainBiomeFrequency + vec3(0.4, -0.7, 1.2));
    float temperature = 0.5 + 0.5 * fbm(warped.zxy * (terrainBiomeFrequency * 1.8) + vec3(-1.0, 0.8, 0.1));
    return clamp(humidity * 0.72 + temperature * 0.28, 0.0, 1.0);
}

float earth_ocean_mask(vec3 n, float continents, float terrain)
{
    float basinNoise = 0.5 + 0.5 * fbm(n.zxy * 2.1 + vec3(-0.7, 0.5, 1.2));
    float shelfNoise = 0.5 + 0.5 * fbm(n.yzx * 3.4 + vec3(1.1, -0.4, 0.2));
    float basinShape = smoothstep(0.24, 0.7, 1.0 - continents) * (0.55 + 0.45 * basinNoise);
    float seaFill = smoothstep(terrainSeaLevel + 0.02, terrainSeaLevel - 0.1, terrain);
    return clamp(seaFill * basinShape * (0.65 + 0.35 * shelfNoise), 0.0, 1.0);
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
    return largeScale * 0.72
        + mediumScale * 0.18
        + (continents - 0.46) * 0.26 * terrainContinentContrast;
}

float terrain_surface_displacement(vec3 n)
{
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

void build_tangent_frame(vec3 normal, out vec3 tangent, out vec3 bitangent)
{
    vec3 helperAxis = abs(normal.y) > 0.82 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    tangent = normalize(cross(helperAxis, normal));
    bitangent = normalize(cross(normal, tangent));
}

float approximate_basin_depth(vec3 n)
{
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

void sample_flooded_region_overlay(vec3 n, out float overlayAlpha, out float boundaryAlpha)
{
    overlayAlpha = 0.0;
    boundaryAlpha = 0.0;
}

vec3 perturb_terrain_normal(vec3 n)
{
    vec3 helperAxis = abs(n.y) > 0.82 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 tangent = normalize(cross(helperAxis, n));
    vec3 bitangent = normalize(cross(n, tangent));

    const float offset = 0.03;
    float strength = terrainReliefStrength;

    float baseHeight = terrain_height(n);
    float tangentHeight = terrain_height(normalize(n + tangent * offset));
    float bitangentHeight = terrain_height(normalize(n + bitangent * offset));

    vec3 gradient = (tangentHeight - baseHeight) * tangent + (bitangentHeight - baseHeight) * bitangent;
    return normalize(n - gradient * strength);
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 localNorm = normalize(LocalNormal);
    mat3 worldNormalBasis = mat3(WorldNormalBasisX, WorldNormalBasisY, WorldNormalBasisZ);
    vec3 terrainNorm = normalize(worldNormalBasis * perturb_terrain_normal(localNorm));
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);

    float terrain = terrain_height(localNorm);
    float floorDisplacement = terrain_surface_displacement(localNorm);
    float continents = continent_mask(localNorm);
    float biome = biome_mask(localNorm);
    float mountains = smoothstep(terrainSeaLevel - 0.02, terrainMountainSharpness, terrain);
    float ridges = pow(1.0 - abs(fbm(localNorm * terrainRidgeFrequency + vec3(0.4, -0.8, 1.1))), 2.4);
    float craters = crater_mask(localNorm * 1.3 + vec3(0.4, -0.6, 1.2));
    float iceCaps = smoothstep(0.62, 0.9, abs(localNorm.y));
    float oceanDepthRaw = clamp((terrainSeaLevel - terrain) / 0.24, 0.0, 1.0) * terrainOceanVisibility;
    if (terrainEarthMacroContinentStrength > 0.001)
        oceanDepthRaw = max(oceanDepthRaw, earth_ocean_mask(localNorm, continents, terrain) * terrainOceanVisibility);
    float oceanDepth = terrainStaticOceanTintEnabled != 0 ? oceanDepthRaw : 0.0;
    float landMask = 1.0 - oceanDepth;
    float landElevation = saturate((terrain - terrainSeaLevel) / max(terrainMountainSharpness - terrainSeaLevel, 0.001)) * landMask;
    float shelfMask = oceanDepth * (1.0 - smoothstep(0.08, 0.42, oceanDepth));
    float vegetationMask = continents * terrainVegetationStrength * smoothstep(0.56, 0.84, biome) * landMask * (1.0 - iceCaps);
    float dryMask = landMask * smoothstep(0.18, 0.6, 1.0 - biome) * (1.0 - vegetationMask);
    float reliefMask = saturate(1.0 - max(dot(terrainNorm, norm), 0.0));
    float terrainShadow = 1.0 - clamp((reliefMask * 1.5 + ridges * 0.2) * landMask, 0.0, 0.44);
    float terrainHighlight = smoothstep(0.18, 0.92, landElevation + ridges * 0.18);

    vec3 rockDark = terrainRockDarkColor;
    vec3 rockMid = terrainRockMidColor;
    vec3 rockBright = terrainRockBrightColor;
    vec3 dustColor = terrainDustColor;
    vec3 iceColor = terrainIceColor;

    vec3 oceanColor = mix(terrainShallowOceanColor, terrainDeepOceanColor, smoothstep(0.08, 0.95, oceanDepth));
    oceanColor = mix(oceanColor, terrainCoastColor * 0.45, shelfMask * 0.32);
    float coastMask = landMask * (1.0 - smoothstep(0.015, 0.12, max(terrain - terrainSeaLevel, 0.0)));
    float interiorMask = continents * landMask * (1.0 - coastMask);
    float mountainMask = mountains * landMask * (0.45 + 0.55 * ridges);

    vec3 baseColor;
    if (terrainEarthMacroContinentStrength > 0.001) {
        vec3 landColor = mix(terrainCoastColor, terrainInteriorColor, smoothstep(0.08, 0.38, max(terrain - terrainSeaLevel, 0.0)));
        landColor *= mix(0.92, 1.12, landElevation);
        landColor = mix(landColor, terrainVegetationColor, vegetationMask * 0.92);
        landColor = mix(landColor, terrainMountainColor, mountainMask);
        landColor = mix(landColor, terrainMountainColor * 1.05, terrainHighlight * 0.55);
        landColor = mix(landColor, dustColor * 0.75, dryMask * 0.18);
        baseColor = mix(oceanColor, landColor, landMask);
        baseColor = mix(baseColor, terrainCoastColor, coastMask * (0.55 + 0.45 * continents));
        baseColor = mix(baseColor, rockDark * 0.55, craters * 0.24 * landMask);
    }
    else {
        vec3 landColor = mix(rockDark, rockMid, smoothstep(0.22, 0.78, biome));
        float dustAmount = dryMask * (0.55 + 0.25 * (1.0 - continents));
        landColor = mix(landColor, dustColor, dustAmount);
        landColor = mix(landColor, rockBright, mountains * 0.78 + ridges * 0.16);
        landColor = mix(landColor, rockBright * 1.05, terrainHighlight * 0.35);
        landColor *= mix(0.94, 1.08, landElevation);
        landColor = mix(landColor, terrainVegetationColor, vegetationMask);
        baseColor = mix(landColor, oceanColor, oceanDepth);
        baseColor = mix(baseColor, rockDark * 0.45, craters * 0.82 * landMask);
    }

    baseColor = mix(baseColor, iceColor, iceCaps * (0.25 + 0.75 * mountains));
    baseColor *= mix(1.0, terrainShadow, 0.72);

    if (terrainDebugMode == 1) {
        float reliefStrength = max(terrainReliefStrength, 0.01);
        float displacementMin = -terrainDisplacementStrength * 0.08;
        float displacementMax = terrainDisplacementStrength * (0.55 + 0.40 * reliefStrength);
        float elevationNormalized = clamp((floorDisplacement - displacementMin) / max(displacementMax - displacementMin, 0.0001), 0.0, 1.0);
        vec3 lowColor = vec3(0.12, 0.08, 0.42);
        vec3 midLowColor = vec3(0.18, 0.48, 0.78);
        vec3 midColor = vec3(0.92, 0.88, 0.28);
        vec3 midHighColor = vec3(0.98, 0.58, 0.18);
        vec3 highColor = vec3(1.0, 0.12, 0.08);
        vec3 veryHighColor = vec3(1.0, 0.98, 0.92);

        vec3 debugColor;
        if (elevationNormalized < 0.25)
            debugColor = mix(lowColor, midLowColor, elevationNormalized * 4.0);
        else if (elevationNormalized < 0.5)
            debugColor = mix(midLowColor, midColor, (elevationNormalized - 0.25) / 0.25);
        else if (elevationNormalized < 0.7)
            debugColor = mix(midColor, midHighColor, (elevationNormalized - 0.5) / 0.2);
        else if (elevationNormalized < 0.85)
            debugColor = mix(midHighColor, highColor, (elevationNormalized - 0.7) / 0.15);
        else
            debugColor = mix(highColor, veryHighColor, (elevationNormalized - 0.85) / 0.15);

        float contourSpacing = max((displacementMax - displacementMin) / 9.0, 0.0008);
        float contourValue = fract((floorDisplacement - displacementMin) / contourSpacing);
        float contourLine = 1.0 - smoothstep(0.0, 0.04, min(contourValue, 1.0 - contourValue));
        debugColor = mix(debugColor, vec3(0.0), contourLine * 0.3);

        baseColor = debugColor;
    }
    else if (terrainDebugMode == 2) {
        float reliefStrength = max(terrainReliefStrength, 0.01);
        float displacementMin = -terrainDisplacementStrength * 0.08;
        float displacementMax = terrainDisplacementStrength * (0.55 + 0.40 * reliefStrength);
        float elevationGray = clamp((floorDisplacement - displacementMin) / max(displacementMax - displacementMin, 0.0001), 0.0, 1.0);
        baseColor = vec3(elevationGray);
    }
    else if (terrainDebugMode == 3) {
        float aboveSeaLevel = step(terrainSeaLevel, terrain);
        vec3 belowColor = vec3(0.08, 0.18, 0.72);
        vec3 aboveColor = vec3(0.18, 0.72, 0.18);
        float heightDiff = abs(terrain - terrainSeaLevel);
        float maskStrength = clamp(heightDiff * 2.0, 0.3, 1.0);
        baseColor = mix(belowColor, aboveColor, aboveSeaLevel) * maskStrength;
    }
    else if (terrainDebugMode == 4) {
        float basinDepth = approximate_basin_depth(localNorm);
        float basinStrength = clamp(basinDepth / max(terrainDisplacementStrength * 0.18, 0.0001), 0.0, 1.0);
        float localMinimum = smoothstep(0.002, 0.008, basinDepth);
        vec3 basinColor = mix(vec3(0.08, 0.08, 0.1), vec3(0.1, 0.72, 1.0), basinStrength);
        basinColor = mix(basinColor, vec3(1.0, 0.88, 0.24), smoothstep(0.45, 0.85, basinStrength));
        baseColor = mix(basinColor, vec3(1.0, 0.18, 0.18), localMinimum * 0.8);
    }
    else if (terrainDebugMode == 5) {
        float overlayAlpha;
        float boundaryAlpha;
        sample_flooded_region_overlay(localNorm, overlayAlpha, boundaryAlpha);
        baseColor = mix(baseColor, vec3(0.08, 0.62, 0.95), overlayAlpha);
        baseColor = mix(baseColor, vec3(0.96, 1.0, 1.0), boundaryAlpha * 0.82);
    }

    float ambient = 0.08;
    float diffuse = max(dot(terrainNorm, lightDir), 0.0);
    float specular = pow(max(dot(viewDir, reflect(-lightDir, terrainNorm)), 0.0), 28.0);
    float rim = pow(1.0 - max(dot(terrainNorm, viewDir), 0.0), 2.2);

    vec3 lightResponse = lightColor * (ambient + diffuse * 1.12);
    vec3 color = baseColor * lightResponse;
    color += lightColor * rockBright * specular * (0.05 + 0.14 * mountains + 0.05 * ridges);
    color += lightColor * terrainMountainColor * terrainHighlight * landMask * 0.08;
    color += lightColor * dustColor * rim * 0.05;

    if (terrainDebugMode != 0)
        color = mix(baseColor, color, 0.18);

    FragColor = vec4(color * intensity, 1.0);
}
