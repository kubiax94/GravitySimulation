#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 systemModel;
uniform mat4 view;
uniform mat4 projection;
uniform float particleSize;
uniform float particleRadius;
uniform float viewportHeight;
uniform int renderPrimitiveMode;
uniform int surfaceInputPass;
uniform int debugVisualizationMode;
uniform int simulationMode;
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
uniform int planetaryPhysicsMaskTextureAvailable;
uniform sampler2D planetaryPhysicsMaskTexture;
uniform int planetaryRenderMaskTextureAvailable;
uniform sampler2D planetaryRenderMaskTexture;
uniform int planetaryWaterLevelTextureAvailable;
uniform sampler2D planetaryWaterLevelTexture;

out vec3 CenterViewPos;
out vec3 DebugColorData;
out vec3 SurfaceWorldPos;
out vec3 SurfaceWorldNormal;
out vec3 ParticleCenterWorldPos;
out vec3 PlanetaryCenterWorldPos;
flat out float PlanetarySolidRadiusWorld;
flat out float ParticleRadiusWorld;
out float WaterDepth01;
out float WaterColumnDepth01;
out float WaterSurfaceBand01;
out float SurfaceCarrierWeight;
flat out float RenderFloodMask;
flat out vec2 SurfaceSpriteAxisMajor;
flat out vec2 SurfaceSpriteAxisMinor;
flat out vec2 SurfaceSpriteScale;
flat out float SurfaceCapsuleBlend;

float terrain_surface_displacement(vec3 n);
float sample_planetary_physics_mask_binary(vec3 normal);
float sample_planetary_render_mask_binary(vec3 normal);
float sample_planetary_render_mask_coverage(vec3 normal);
float sample_planetary_water_level(vec3 normal);

void compute_local_water_data(vec3 localPosition, out float depth01, out float columnDepth01, out float surfaceBand01, out vec3 projectedSurfacePosition) {
    if (simulationMode != 1)
    {
        depth01 = 0.0;
        columnDepth01 = 0.0;
        surfaceBand01 = 0.0;
        projectedSurfacePosition = localPosition;
        return;
    }

    vec3 radial = localPosition - planetaryCenter;
    float radialDistance = length(radial);
    vec3 normal = radialDistance > 0.000001 ? radial / radialDistance : vec3(0.0, 1.0, 0.0);
    float floorRadius = planetaryRadius + terrain_surface_displacement(normal);
    float floodMask = sample_planetary_physics_mask_binary(normal);
    float waterLevel = sample_planetary_water_level(normal);
    float globalCeilingRadius = min(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness);
    float ceilingRadius = floorRadius;
    if (floodMask >= 0.5 && waterLevel >= 0.0) {
        float localWaterSurfaceRadius = mix(planetaryRadius, planetaryRadius + planetaryShellThickness, waterLevel);
        ceilingRadius = clamp(localWaterSurfaceRadius, floorRadius, globalCeilingRadius);
    }
    float localDepth = max(ceilingRadius - radialDistance, 0.0);
    float availableDepth = max(ceilingRadius - floorRadius, 0.0);
    depth01 = availableDepth > 0.000001 ? clamp(localDepth / availableDepth, 0.0, 1.0) : 0.0;
    columnDepth01 = smoothstep(particleRadius * 0.55, particleRadius * 4.2, availableDepth);
    surfaceBand01 = availableDepth > 0.000001
        ? 1.0 - smoothstep(particleRadius * 0.18, max(particleRadius * 1.65, availableDepth * 0.18), localDepth)
        : 0.0;

    float surfaceInset = availableDepth > 0.000001
        ? min(max(particleRadius * 0.08, availableDepth * 0.06), max(particleRadius * 0.22, availableDepth * 0.18))
        : particleRadius * 0.08;
    float projectedRadius = clamp(ceilingRadius - surfaceInset, floorRadius, ceilingRadius);
    projectedSurfacePosition = planetaryCenter + normal * projectedRadius;
}

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

mat3 build_rotation_only_matrix() {
    vec3 basisX = normalize(vec3(systemModel[0]));
    vec3 basisY = normalize(vec3(systemModel[1]));
    vec3 basisZ = normalize(vec3(systemModel[2]));
    return mat3(basisX, basisY, basisZ);
}

float earth_ocean_mask(vec3 n, float continents, float terrain) {
    float basinNoise = 0.5 + 0.5 * fbm(n.zxy * 2.1 + vec3(-0.7, 0.5, 1.2));
    float shelfNoise = 0.5 + 0.5 * fbm(n.yzx * 3.4 + vec3(1.1, -0.4, 0.2));
    float basinShape = smoothstep(0.24, 0.7, 1.0 - continents) * (0.55 + 0.45 * basinNoise);
    float seaFill = smoothstep(terrainSeaLevel + 0.02, terrainSeaLevel - 0.1, terrain);
    return clamp(seaFill * basinShape * (0.65 + 0.35 * shelfNoise), 0.0, 1.0);
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

float sample_planetary_physics_mask_binary(vec3 normal) {
    if (planetaryPhysicsMaskTextureAvailable == 0)
        return -1.0;

    return textureLod(planetaryPhysicsMaskTexture, build_planetary_hydrology_uv(normal), 0.0).r;
}

float sample_planetary_render_mask_binary(vec3 normal) {
    if (planetaryRenderMaskTextureAvailable == 0)
        return -1.0;

    return textureLod(planetaryRenderMaskTexture, build_planetary_hydrology_uv(normal), 0.0).r;
}

float sample_planetary_render_mask_coverage(vec3 normal) {
    if (planetaryRenderMaskTextureAvailable == 0)
        return -1.0;

    vec2 uv = build_planetary_hydrology_uv(normal);
    vec2 texel = 1.0 / vec2(textureSize(planetaryRenderMaskTexture, 0));
    const vec2 offsets[8] = vec2[8](
        vec2(1.0, 0.0),
        vec2(-1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, -1.0),
        vec2(1.0, 1.0),
        vec2(-1.0, 1.0),
        vec2(1.0, -1.0),
        vec2(-1.0, -1.0));
    const float weights[8] = float[8](0.12, 0.12, 0.12, 0.12, 0.07, 0.07, 0.07, 0.07);

    float coverage = textureLod(planetaryRenderMaskTexture, uv, 0.0).r * 0.24;
    for (int i = 0; i < 8; ++i) {
        vec2 sampleUv = uv + offsets[i] * texel;
        sampleUv.x = fract(sampleUv.x + 1.0);
        sampleUv.y = clamp(sampleUv.y, texel.y * 0.5, 1.0 - texel.y * 0.5);
        coverage += textureLod(planetaryRenderMaskTexture, sampleUv, 0.0).r * weights[i];
    }

    return clamp(coverage, 0.0, 1.0);
}

float sample_planetary_water_level(vec3 normal) {
    if (planetaryWaterLevelTextureAvailable == 0)
        return -1.0;

    return textureLod(planetaryWaterLevelTexture, build_planetary_hydrology_uv(normal), 0.0).r;
}

float planetary_ocean_fill(vec3 localPosition) {
    if (simulationMode != 1 || planetaryTerrainEnabled == 0)
        return 1.0;

    vec3 radial = localPosition - planetaryCenter;
    float radialSq = dot(radial, radial);
    vec3 normal = radialSq > 0.000001 ? radial * inversesqrt(radialSq) : vec3(0.0, 1.0, 0.0);
    float floodMask = sample_planetary_render_mask_binary(normal);
    if (floodMask >= 0.0)
        return floodMask;

    float continents = continent_mask(normal);
    float terrain = terrain_height(normal);
    float oceanFill = clamp((terrainSeaLevel - terrain) / 0.24, 0.0, 1.0);
    if (terrainEarthMacroContinentStrength > 0.001)
        oceanFill = max(oceanFill, earth_ocean_mask(normal, continents, terrain));

    return smoothstep(0.02, 0.18, oceanFill);
}

float sample_render_flood_mask(vec3 localPosition) {
    if (simulationMode != 1)
        return clamp(planetary_ocean_fill(localPosition), 0.0, 1.0);

    vec3 radial = localPosition - planetaryCenter;
    float radialSq = dot(radial, radial);
    vec3 normal = radialSq > 0.000001 ? radial * inversesqrt(radialSq) : vec3(0.0, 1.0, 0.0);
    float floodMask = sample_planetary_render_mask_coverage(normal);
    if (floodMask >= 0.0)
        return clamp(floodMask, 0.0, 1.0);

    return clamp(planetary_ocean_fill(localPosition), 0.0, 1.0);
}

vec2 safe_normalize_vec2(vec2 v) {
    float len = length(v);
    return len > 0.000001 ? v / len : vec2(1.0, 0.0);
}

void main() {
    FluidParticle particle = particles[gl_InstanceID];
    vec3 renderCenterLocalPos = particle.position.xyz;
    vec3 projectedSurfaceLocalPos = particle.position.xyz;
    vec3 surfaceLocalNormal = vec3(0.0, 1.0, 0.0);
    if (simulationMode == 1) {
        vec3 radial = particle.position.xyz - planetaryCenter;
        float radialSq = dot(radial, radial);
        surfaceLocalNormal = radialSq > 0.000001 ? radial * inversesqrt(radialSq) : vec3(0.0, 1.0, 0.0);

        if (renderPrimitiveMode != 0) {
            float radialComponent = dot(aPos, surfaceLocalNormal);
            vec3 tangentOffset = aPos - surfaceLocalNormal * radialComponent;
            float capHeight = max(radialComponent, -0.32) * 0.14 - 0.08;
            vec3 flattenedOffset = tangentOffset * 0.78 + surfaceLocalNormal * capHeight;
            renderCenterLocalPos = particle.position.xyz + flattenedOffset * particleRadius;
        }
    }

    compute_local_water_data(particle.position.xyz, WaterDepth01, WaterColumnDepth01, WaterSurfaceBand01, projectedSurfaceLocalPos);
    RenderFloodMask = sample_render_flood_mask(particle.position.xyz);
    SurfaceCarrierWeight = 0.0;
    if (simulationMode == 1 && surfaceInputPass != 0) {
        SurfaceCarrierWeight = clamp(
            (0.18 + 0.82 * smoothstep(0.03, 0.22, RenderFloodMask))
            * (0.38 + 0.62 * smoothstep(0.02, 0.24, WaterColumnDepth01))
            * (0.55 + 0.45 * smoothstep(0.01, 0.20, WaterSurfaceBand01)),
            0.0,
            1.0);
        renderCenterLocalPos = projectedSurfaceLocalPos;
    }

    vec3 particleVertex = renderCenterLocalPos + aPos * particleRadius;
    vec4 worldPos = systemModel * vec4(particleVertex, 1.0);
    ParticleCenterWorldPos = (systemModel * vec4(renderCenterLocalPos, 1.0)).xyz;
    PlanetaryCenterWorldPos = (systemModel * vec4(planetaryCenter, 1.0)).xyz;
    float systemScale = max(max(length(vec3(systemModel[0])), length(vec3(systemModel[1]))), length(vec3(systemModel[2])));
    PlanetarySolidRadiusWorld = planetaryRadius * systemScale;
    ParticleRadiusWorld = particleRadius * systemScale;
    vec4 viewPos = view * worldPos;
    gl_Position = projection * viewPos;

    SurfaceWorldPos = worldPos.xyz;
    SurfaceWorldNormal = simulationMode == 1
        ? normalize(mat3(systemModel) * surfaceLocalNormal)
        : normalize(mat3(systemModel) * aNormal);
    SurfaceSpriteAxisMajor = vec2(1.0, 0.0);
    SurfaceSpriteAxisMinor = vec2(0.0, 1.0);
    SurfaceSpriteScale = vec2(1.0);
    SurfaceCapsuleBlend = 0.0;

    if (renderPrimitiveMode == 0) {
        float viewDepth = max(-viewPos.z, 0.001);
        float projectedDiameter = projection[1][1] * particleRadius * viewportHeight / viewDepth;
        float surfaceCoverage = clamp(RenderFloodMask * 0.12 + WaterColumnDepth01 * 0.14 + WaterSurfaceBand01 * 0.18 + SurfaceCarrierWeight * 0.40, 0.0, 1.0);
        float fillScale = simulationMode == 1
            ? (surfaceInputPass != 0
                ? 3.1 + particleSize * 0.42 + surfaceCoverage * 0.34
                : 1.7 + particleSize * 0.3)
            : 1.0 + particleSize * 0.35;
        float minPointSize = simulationMode == 1
            ? (surfaceInputPass != 0 ? max(particleSize * 1.8, 6.0) : max(particleSize * 1.3, 3.0))
            : particleSize;
        float maxPointSize = simulationMode == 1
            ? (surfaceInputPass != 0 ? max(particleSize * 4.4, 24.0) : max(particleSize * 3.6, 18.0))
            : max(particleSize * 4.0, 24.0);
        gl_PointSize = clamp(projectedDiameter * fillScale, minPointSize, maxPointSize);

        if (simulationMode == 1 && surfaceInputPass != 0) {
            vec3 worldNormal = normalize(mat3(systemModel) * surfaceLocalNormal);
            vec3 helperAxis = abs(worldNormal.y) > 0.82 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
            vec3 worldTangent = normalize(cross(helperAxis, worldNormal));
            vec3 worldBitangent = normalize(cross(worldNormal, worldTangent));
            vec3 centerView = viewPos.xyz;
            vec3 tangentView = (view * vec4(ParticleCenterWorldPos + worldTangent * ParticleRadiusWorld, 1.0)).xyz - centerView;
            vec3 bitangentView = (view * vec4(ParticleCenterWorldPos + worldBitangent * ParticleRadiusWorld, 1.0)).xyz - centerView;
            vec2 tangentScreen = vec2(projection[0][0] * tangentView.x, projection[1][1] * tangentView.y);
            vec2 bitangentScreen = vec2(projection[0][0] * bitangentView.x, projection[1][1] * bitangentView.y);
            float tangentLength = length(tangentScreen);
            float bitangentLength = length(bitangentScreen);
            vec2 majorAxisScreen = tangentScreen;
            vec2 minorAxisScreen = bitangentScreen;
            float majorLength = tangentLength;
            float minorLength = bitangentLength;
            if (bitangentLength > tangentLength) {
                majorAxisScreen = bitangentScreen;
                minorAxisScreen = tangentScreen;
                majorLength = bitangentLength;
                minorLength = tangentLength;
            }

            SurfaceSpriteAxisMajor = safe_normalize_vec2(majorAxisScreen);
            SurfaceSpriteAxisMinor = minorLength > 0.000001
                ? safe_normalize_vec2(minorAxisScreen)
                : vec2(-SurfaceSpriteAxisMajor.y, SurfaceSpriteAxisMajor.x);

            float anisotropy = clamp(majorLength / max(minorLength, 0.000001), 1.0, 1.9);
            float majorScale = mix(0.98, 1.46, surfaceCoverage) * mix(1.0, 1.38, clamp(anisotropy - 1.0, 0.0, 1.0));
            float minorScale = mix(0.80, 0.98, surfaceCoverage);
            SurfaceSpriteScale = vec2(majorScale, minorScale);
            SurfaceCapsuleBlend = clamp(smoothstep(1.04, 1.42, anisotropy) * (0.24 + surfaceCoverage * 0.62), 0.0, 1.0);
        }
    }
    else
        gl_PointSize = 1.0;

    CenterViewPos = viewPos.xyz;

    DebugColorData = vec3(0.0);
    mat3 surfaceFrame = build_rotation_only_matrix();
    vec3 renderedLocalPosition = surfaceFrame * particle.position.xyz;
    if (debugVisualizationMode == 1)
        DebugColorData.x = RenderFloodMask;
    else if (debugVisualizationMode == 2)
        DebugColorData.x = particle.solver_data.z;
    else if (debugVisualizationMode == 3)
        DebugColorData.x = particle.solver_data.x;
    else if (debugVisualizationMode == 4)
        DebugColorData = mat3(systemModel) * particle.velocity.xyz;
    else if ((debugVisualizationMode == 5 || debugVisualizationMode == 6) && simulationMode == 1) {
        vec3 radial = renderedLocalPosition - planetaryCenter;
        float radialDistance = length(radial);
        vec3 normal = radialDistance > 0.000001 ? radial / radialDistance : vec3(0.0, 1.0, 0.0);
        float floorRadius = planetaryRadius + terrain_surface_displacement(normal);
        float floorClearance = max(radialDistance - floorRadius, 0.0);
        float waterSurfaceDistance = max(planetaryWaterSurfaceRadius - radialDistance, 0.0);
        DebugColorData.x = debugVisualizationMode == 5 ? floorClearance : waterSurfaceDistance;
    }
    else if (debugVisualizationMode == 7)
        DebugColorData.x = particle.solver_data.w;
    else if (debugVisualizationMode == 8)
        DebugColorData.x = particle.debug_data.w;
    else if (debugVisualizationMode == 9)
        DebugColorData = mat3(systemModel) * particle.debug_data.xyz;
}
