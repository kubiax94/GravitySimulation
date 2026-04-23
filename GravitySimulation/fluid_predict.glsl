#version 460
layout(local_size_x = 64) in;

uniform float dt;
uniform vec3 gravity;
uniform vec3 boundsMin;
uniform vec3 boundsMax;
uniform float restitution;
uniform float collisionDamping;
uniform float interactionRadius;
uniform float particleRadius;
uniform float separationStrength;
uniform float nearPressureStrength;
uniform float velocityDamping;
uniform float viscosityStrength;
uniform float restDensity;
uniform float cellSize;
uniform int gridSizeX;
uniform int gridSizeY;
uniform int gridSizeZ;
uniform int passMode;
uniform int simulationMode;
uniform vec3 planetaryCenter;
uniform float planetaryRadius;
uniform float planetaryShellThickness;
uniform float planetaryWaterSurfaceRadius;
uniform float planetaryGravityStrength;
uniform float planetaryDownslopeStrength;
uniform float planetaryBottomFriction;
uniform float planetaryBottomNormalDamping;
uniform float planetaryFloorAttractionStrength;
uniform float planetaryFloodGuidanceStrength;
uniform float planetarySurfaceLayerThicknessScale;
uniform float planetarySurfaceLayerAttractionStrength;
uniform float planetarySurfaceLayerNormalVelocityDamping;
uniform vec3 planetaryAngularVelocity;
uniform float planetaryCoriolisStrength;
uniform float planetaryTidalStrength;
uniform int planetaryExternalGravitySourceCount;
uniform vec4 planetaryExternalGravitySources[8];
uniform vec3 planetarySurfaceFrameX;
uniform vec3 planetarySurfaceFrameY;
uniform vec3 planetarySurfaceFrameZ;
uniform int planetaryPhysicsMaskTextureAvailable;
uniform sampler2D planetaryPhysicsMaskTexture;
uniform int planetaryWaterLevelTextureAvailable;
uniform sampler2D planetaryWaterLevelTexture;
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

struct FluidParticle {
    vec4 position;
    vec4 velocity;
    vec4 predicted_position;
    vec4 delta_position;
    vec4 solver_data;
    vec4 debug_data;
};

layout(std430, binding = 0) buffer FluidParticles {
    FluidParticle particles[];
};

layout(std430, binding = 1) buffer FluidGridCellHeads {
    int cellHeads[];
};

layout(std430, binding = 2) buffer FluidGridNextParticle {
    int nextParticle[];
};

layout(std430, binding = 3) buffer RespawnCandidateCount {
    uint respawnCandidateCount;
};

layout(std430, binding = 4) buffer RespawnCandidateIndices {
    uint respawnCandidateIndices[];
};

const float eps = 0.0001;
const float epsSq = eps * eps;
const float constraintRelaxation = 0.05;
const uint maxRespawnCandidateCount = 2048u;

vec3 clamp_vector_length(vec3 v, float maxLength) {
    float lenSq = dot(v, v);
    float maxLengthSafe = max(maxLength, eps);
    if (lenSq > maxLengthSafe * maxLengthSafe)
        return v * (maxLengthSafe * inversesqrt(lenSq));

    return v;
}

vec3 project_onto_plane(vec3 v, vec3 normal) {
    return v - normal * dot(v, normal);
}

mat3 planetary_surface_from_simulation() {
    return mat3(planetarySurfaceFrameX, planetarySurfaceFrameY, planetarySurfaceFrameZ);
}

vec3 to_planetary_surface_frame(vec3 direction) {
    vec3 transformed = planetary_surface_from_simulation() * direction;
    float lenSq = dot(transformed, transformed);
    return lenSq > epsSq ? transformed * inversesqrt(lenSq) : vec3(0.0, 1.0, 0.0);
}

vec3 from_planetary_surface_frame(vec3 direction) {
    vec3 transformed = transpose(planetary_surface_from_simulation()) * direction;
    float lenSq = dot(transformed, transformed);
    return lenSq > epsSq ? transformed * inversesqrt(lenSq) : vec3(0.0, 1.0, 0.0);
}

vec2 build_planetary_hydrology_uv(vec3 surfaceNormal) {
    vec3 safeNormal = dot(surfaceNormal, surfaceNormal) > epsSq
        ? normalize(surfaceNormal)
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

    vec3 surfaceNormal = to_planetary_surface_frame(normal);
    return textureLod(planetaryPhysicsMaskTexture, build_planetary_hydrology_uv(surfaceNormal), 0.0).r;
}

float sample_planetary_water_level(vec3 normal) {
    if (planetaryWaterLevelTextureAvailable == 0)
        return -1.0;

    vec3 surfaceNormal = to_planetary_surface_frame(normal);
    return textureLod(planetaryWaterLevelTexture, build_planetary_hydrology_uv(surfaceNormal), 0.0).r;
}

float compute_planetary_surface_layer_radius(float floorRadius, float ceilingRadius) {
    float fluidDepth = max(ceilingRadius - floorRadius, 0.0);
    if (fluidDepth <= eps)
        return floorRadius;

    float retainedLayerDepth = clamp(
        max(fluidDepth * planetarySurfaceLayerThicknessScale, particleRadius * 1.5),
        particleRadius * 1.5,
        fluidDepth);
    float shallowLayerBlend = 1.0 - smoothstep(particleRadius * 2.0, particleRadius * 5.5, fluidDepth);
    float deepLayerCenter = floorRadius + fluidDepth * 0.32;
    float shallowLayerCenter = ceilingRadius - retainedLayerDepth * 0.82;
    float layerCenter = mix(deepLayerCenter, shallowLayerCenter, shallowLayerBlend);
    return clamp(layerCenter, floorRadius + particleRadius * 0.65, ceilingRadius - particleRadius * 0.35);
}

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
    float height = largeScale * 0.72
        + mediumScale * 0.18
        + (continents - 0.46) * 0.26 * terrainContinentContrast;

    return height;
}

float planetary_terrain_surface_offset(vec3 normal) {
    if (planetaryTerrainEnabled == 0)
        return 0.0;

    float macroHeight = terrain_macro_height(normal);
    float fullHeight = terrain_height(normal);
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

float sample_planetary_floor_radius(vec3 normal) {
    vec3 surfaceNormal = to_planetary_surface_frame(normal);
    return planetaryRadius + planetary_terrain_surface_offset(surfaceNormal);
}

vec3 sample_planetary_surface_normal(vec3 radialNormal) {
    vec3 surfaceRadialNormal = to_planetary_surface_frame(radialNormal);
    vec3 tangent;
    vec3 bitangent;
    build_tangent_frame(surfaceRadialNormal, tangent, bitangent);

    float sampleOffset = max(0.025, particleRadius * 2.5);
    vec3 centerPoint = surfaceRadialNormal * (planetaryRadius + planetary_terrain_surface_offset(surfaceRadialNormal));
    vec3 tangentNormal = normalize(surfaceRadialNormal + tangent * sampleOffset);
    vec3 bitangentNormal = normalize(surfaceRadialNormal + bitangent * sampleOffset);
    vec3 tangentPoint = tangentNormal * (planetaryRadius + planetary_terrain_surface_offset(tangentNormal));
    vec3 bitangentPoint = bitangentNormal * (planetaryRadius + planetary_terrain_surface_offset(bitangentNormal));
    vec3 surfaceNormal = normalize(cross(tangentPoint - centerPoint, bitangentPoint - centerPoint));
    if (dot(surfaceNormal, surfaceRadialNormal) < 0.0)
        surfaceNormal = -surfaceNormal;

    return from_planetary_surface_frame(surfaceNormal);
}

float planetary_ocean_fill(vec3 normal);

void get_planetary_topology_limits(vec3 radialNormal, out float floorRadius, out float ceilingRadius, out float floodMask, out float floodCoverage) {
    floorRadius = sample_planetary_floor_radius(radialNormal);
    floodMask = sample_planetary_physics_mask_binary(radialNormal);
    floodCoverage = sample_planetary_water_level(radialNormal);

    if (floodMask >= 0.0 && floodCoverage >= 0.0) {
        float globalCeilingRadius = min(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness);
        float localWaterSurfaceRadius = mix(planetaryRadius, planetaryRadius + planetaryShellThickness, floodCoverage);
        ceilingRadius = clamp(localWaterSurfaceRadius, floorRadius, globalCeilingRadius);
        return;
    }

    floodMask = planetary_ocean_fill(radialNormal);
    floodCoverage = floodMask;

    float globalCeilingRadius = min(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness);
    float availableDepth = max(globalCeilingRadius - floorRadius, 0.0);
    float localFloodFactor = floodMask >= 0.5
        ? smoothstep(0.18, 0.96, floodCoverage)
        : 0.0;
    float minimumFloodDepth = floodMask >= 0.5 ? particleRadius * 0.75 : 0.0;
    ceilingRadius = floorRadius + max(availableDepth * localFloodFactor, minimumFloodDepth);
    ceilingRadius = clamp(ceilingRadius, floorRadius, globalCeilingRadius);
}

void get_planetary_flow_data(vec3 position, out vec3 radialNormal, out vec3 terrainNormal, out float floorRadius, out float ceilingRadius, out float floorClearance, out float bottomInfluence) {
    vec3 radial = position - planetaryCenter;
    float radialSq = dot(radial, radial);
    radialNormal = radialSq > epsSq ? radial * inversesqrt(radialSq) : vec3(0.0, 1.0, 0.0);
    float floodMask;
    float floodCoverage;
    get_planetary_topology_limits(radialNormal, floorRadius, ceilingRadius, floodMask, floodCoverage);
    float fluidDepth = max(ceilingRadius - floorRadius, 0.0);
    float distance = sqrt(max(radialSq, epsSq));
    floorClearance = max(distance - floorRadius, 0.0);
    float groundContact = 1.0 - smoothstep(0.0, particleRadius * 1.35, floorClearance);
    bottomInfluence = fluidDepth > particleRadius * 0.35
        ? 1.0 - clamp(floorClearance / fluidDepth, 0.0, 1.0)
        : groundContact;
    bottomInfluence = max(bottomInfluence, groundContact);
    terrainNormal = sample_planetary_surface_normal(radialNormal);
}

float earth_ocean_mask(vec3 n, float continents, float terrain) {
    float basinNoise = 0.5 + 0.5 * fbm(n.zxy * 2.1 + vec3(-0.7, 0.5, 1.2));
    float shelfNoise = 0.5 + 0.5 * fbm(n.yzx * 3.4 + vec3(1.1, -0.4, 0.2));
    float basinShape = smoothstep(0.24, 0.7, 1.0 - continents) * (0.55 + 0.45 * basinNoise);
    float seaFill = smoothstep(terrainSeaLevel + 0.02, terrainSeaLevel - 0.1, terrain);
    return clamp(seaFill * basinShape * (0.65 + 0.35 * shelfNoise), 0.0, 1.0);
}

float planetary_ocean_fill(vec3 normal) {
    if (planetaryTerrainEnabled == 0)
        return 1.0;

    float floodMask = sample_planetary_physics_mask_binary(normal);
    if (floodMask >= 0.0)
        return floodMask;

    vec3 surfaceNormal = to_planetary_surface_frame(normal);
    float continents = continent_mask(surfaceNormal);
    float terrain = terrain_height(surfaceNormal);
    float oceanFill = clamp((terrainSeaLevel - terrain) / 0.24, 0.0, 1.0);
    if (terrainEarthMacroContinentStrength > 0.001)
        oceanFill = max(oceanFill, earth_ocean_mask(surfaceNormal, continents, terrain));

    return smoothstep(0.02, 0.18, oceanFill);
}

float sample_planetary_basin_potential(vec3 radialNormal) {
    float floorRadius = sample_planetary_floor_radius(radialNormal);
    float oceanFill = sample_planetary_physics_mask_binary(radialNormal);
    if (oceanFill < 0.0)
        oceanFill = planetary_ocean_fill(radialNormal);
    float coastalDryness = 1.0 - oceanFill;
    float dryPenalty = coastalDryness * coastalDryness * max(planetaryShellThickness * 0.65, particleRadius * 3.5);
    return floorRadius + dryPenalty;
}

void sample_planetary_basin_direction(vec3 radialNormal, out float basinWeight, out vec3 basinDirection) {
    basinWeight = 0.0;
    basinDirection = vec3(0.0);

    vec3 surfaceNormal = to_planetary_surface_frame(radialNormal);
    vec3 tangent;
    vec3 bitangent;
    build_tangent_frame(surfaceNormal, tangent, bitangent);

    float sampleOffset = clamp(max(interactionRadius, particleRadius * 4.0), 0.03, 0.2);
    vec3 posTangent = normalize(surfaceNormal + tangent * sampleOffset);
    vec3 negTangent = normalize(surfaceNormal - tangent * sampleOffset);
    vec3 posBitangent = normalize(surfaceNormal + bitangent * sampleOffset);
    vec3 negBitangent = normalize(surfaceNormal - bitangent * sampleOffset);

    float centerPotential = sample_planetary_basin_potential(radialNormal);
    float tangentPosPotential = sample_planetary_basin_potential(from_planetary_surface_frame(posTangent));
    float tangentNegPotential = sample_planetary_basin_potential(from_planetary_surface_frame(negTangent));
    float bitangentPosPotential = sample_planetary_basin_potential(from_planetary_surface_frame(posBitangent));
    float bitangentNegPotential = sample_planetary_basin_potential(from_planetary_surface_frame(negBitangent));

    vec3 gradientSurface = tangent * ((tangentPosPotential - tangentNegPotential) / max(sampleOffset * 2.0, eps))
        + bitangent * ((bitangentPosPotential - bitangentNegPotential) / max(sampleOffset * 2.0, eps));
    float gradientLengthSq = dot(gradientSurface, gradientSurface);
    if (gradientLengthSq <= epsSq)
        return;

    vec3 downhillSurface = -gradientSurface * inversesqrt(gradientLengthSq);
    basinDirection = from_planetary_surface_frame(downhillSurface);

    float bestNeighborPotential = min(min(tangentPosPotential, tangentNegPotential), min(bitangentPosPotential, bitangentNegPotential));
    float escapePotential = max(centerPotential - bestNeighborPotential, 0.0);
    float oceanFill = sample_planetary_physics_mask_binary(radialNormal);
    if (oceanFill < 0.0)
        oceanFill = planetary_ocean_fill(radialNormal);
    float dryBias = 1.0 - smoothstep(0.22, 0.78, oceanFill);
    float reliefBias = smoothstep(0.0015, max(planetaryShellThickness * 0.18, particleRadius * 1.1), escapePotential);
    float floodedRetentionBias = smoothstep(0.35, 0.9, oceanFill) * 0.22;
    basinWeight = clamp(reliefBias * max(dryBias, floodedRetentionBias), 0.0, 1.0);
}

ivec3 compute_cell_coords(vec3 position) {
    vec3 relative = (position - boundsMin) / max(cellSize, eps);
    return clamp(ivec3(floor(relative)), ivec3(0), ivec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1));
}

int flatten_cell_index(ivec3 cell) {
    return cell.x + cell.y * gridSizeX + cell.z * gridSizeX * gridSizeY;
}

float density_kernel(float distance) {
    float q = 1.0 - clamp(distance / max(interactionRadius, eps), 0.0, 1.0);
    return q * q * q;
}

vec3 gradient_kernel(vec3 delta, float distance) {
    if (distance <= eps || distance >= interactionRadius)
        return vec3(0.0);

    float q = 1.0 - clamp(distance / max(interactionRadius, eps), 0.0, 1.0);
    return -(3.0 * q * q / max(interactionRadius, eps)) * (delta / distance);
}

float tensile_correction(float distance) {
    float q = 1.0 - clamp(distance / max(interactionRadius, eps), 0.0, 1.0);
    float q2 = q * q;
    return -nearPressureStrength * q2 * q2;
}

vec3 compute_external_acceleration(vec3 position, vec3 velocity, out vec4 debugData, out float coriolisStrengthOut) {
    debugData = vec4(0.0);
    coriolisStrengthOut = 0.0;
    if (simulationMode == 1) {
        vec3 toCenter = planetaryCenter - position;
        float distanceSq = dot(toCenter, toCenter);
        if (distanceSq > epsSq) {
            float distance = sqrt(distanceSq);
            vec3 radialGravity = toCenter * inversesqrt(distanceSq) * planetaryGravityStrength;
            vec3 tidalAcceleration = vec3(0.0);
            for (int sourceIndex = 0; sourceIndex < planetaryExternalGravitySourceCount; ++sourceIndex) {
                vec4 source = planetaryExternalGravitySources[sourceIndex];
                vec3 sourceToParticle = source.xyz - position;
                vec3 sourceToCenter = source.xyz - planetaryCenter;
                float particleDistSq = dot(sourceToParticle, sourceToParticle);
                float centerDistSq = dot(sourceToCenter, sourceToCenter);
                if (particleDistSq <= epsSq || centerDistSq <= epsSq)
                    continue;

                float particleInvDist = inversesqrt(particleDistSq);
                float centerInvDist = inversesqrt(centerDistSq);
                tidalAcceleration += source.w * (
                    sourceToParticle * (particleInvDist * particleInvDist * particleInvDist)
                    - sourceToCenter * (centerInvDist * centerInvDist * centerInvDist));
            }
            tidalAcceleration *= planetaryTidalStrength;

            if (planetaryTerrainEnabled == 0) {
                debugData.w = length(tidalAcceleration);
                return radialGravity + tidalAcceleration;
            }

            vec3 outwardNormal;
            vec3 terrainNormal;
            float floorRadius;
            float oceanCeiling;
            float floorClearance;
            float bottomInfluence;
            get_planetary_flow_data(position, outwardNormal, terrainNormal, floorRadius, oceanCeiling, floorClearance, bottomInfluence);
            float basinWeight;
            vec3 basinDirection;
            sample_planetary_basin_direction(outwardNormal, basinWeight, basinDirection);
            vec3 downslopeGravity = radialGravity - terrainNormal * dot(radialGravity, terrainNormal);
            float columnDepth = max(oceanCeiling - floorRadius, particleRadius);
            float ceilingClearance = max(oceanCeiling - distance, 0.0);
            float topBoundaryInfluence = 1.0 - smoothstep(0.0, particleRadius * 1.4, ceilingClearance);
            vec3 floorAttraction = -outwardNormal * planetaryGravityStrength * topBoundaryInfluence * planetaryFloorAttractionStrength * 0.18;
            float oceanFill = sample_planetary_physics_mask_binary(outwardNormal);
            if (oceanFill < 0.0)
                oceanFill = planetary_ocean_fill(outwardNormal);
            float dryRegion = 1.0 - smoothstep(0.12, 0.78, oceanFill);
            vec3 tangentialVelocity = project_onto_plane(velocity, outwardNormal);
            vec3 coriolisAcceleration = -2.0 * cross(planetaryAngularVelocity, tangentialVelocity) * planetaryCoriolisStrength;
            coriolisAcceleration = project_onto_plane(coriolisAcceleration, outwardNormal);
            coriolisStrengthOut = length(coriolisAcceleration);
            float columnFlowInfluence = oceanFill >= 0.5
                ? mix(0.28, 0.82, bottomInfluence)
                : bottomInfluence;
            vec3 basinFlow = basinDirection
                * (planetaryGravityStrength
                    * planetaryDownslopeStrength
                    * (0.08 + 0.12 * planetaryFloodGuidanceStrength)
                    * basinWeight
                    * mix(1.0, 0.38, oceanFill));

            vec3 tangentialDrive = downslopeGravity * columnFlowInfluence * planetaryDownslopeStrength
                + basinFlow * mix(0.55, 1.0, dryRegion);
            tangentialDrive = project_onto_plane(tangentialDrive, outwardNormal);
            tangentialDrive = clamp_vector_length(tangentialDrive, planetaryGravityStrength * mix(0.12, 0.24, dryRegion));
            vec3 tidalTangential = project_onto_plane(tidalAcceleration, outwardNormal);
            vec3 combinedFlow = tangentialDrive + coriolisAcceleration + tidalTangential;
            debugData = vec4(combinedFlow, length(tidalTangential));

            float layerRadius = compute_planetary_surface_layer_radius(floorRadius, oceanCeiling);
            float layerOffset = distance - layerRadius;
            float layerInfluence = smoothstep(particleRadius * 0.35, particleRadius * 1.6, columnDepth);
            float radialVelocity = dot(velocity, outwardNormal);
            float nearLayer = 1.0 - smoothstep(particleRadius * 0.25, particleRadius * 1.8, abs(layerOffset));
            float layerRetention = layerInfluence * mix(1.0, 0.22, bottomInfluence);
            float clampedLayerOffset = clamp(layerOffset, -particleRadius * 1.6, particleRadius * 1.6);
            vec3 layerAttraction = -outwardNormal
                * clampedLayerOffset
                * planetaryGravityStrength
                * planetarySurfaceLayerAttractionStrength
                * layerRetention
                * 1.05;
            vec3 layerDamping = -outwardNormal
                * radialVelocity
                * clamp(0.05 + planetarySurfaceLayerNormalVelocityDamping * mix(0.08, 0.42, nearLayer) * layerRetention, 0.0, 0.58)
                / max(dt, 0.012);

            return radialGravity
                + tidalAcceleration
                + coriolisAcceleration
                + floorAttraction
                + tangentialDrive
                + layerAttraction
                + layerDamping;
        }

        return vec3(0.0);
    }

    return gravity;
}

void constrain_to_planetary_shell(inout vec3 position) {
    vec3 offset = position - planetaryCenter;
    float distance = length(offset);
    vec3 normal = distance > eps ? offset / distance : vec3(0.0, 1.0, 0.0);
    float minRadius = max(sample_planetary_floor_radius(normal) + particleRadius * 0.18, eps);
    float maxRadius = max(minRadius, min(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness) - particleRadius * 0.15);
    position = planetaryCenter + normal * clamp(distance, minRadius, maxRadius);
}

void get_planetary_shell_data(vec3 position, out vec3 normal, out float distance, out float minRadius, out float maxRadius) {
    vec3 offset = position - planetaryCenter;
    distance = length(offset);
    normal = distance > eps ? offset / distance : vec3(0.0, 1.0, 0.0);
    float floodMask;
    float floodCoverage;
    float ceilingRadius;
    float floorRadius;
    get_planetary_topology_limits(normal, floorRadius, ceilingRadius, floodMask, floodCoverage);
    minRadius = max(sample_planetary_floor_radius(normal) + particleRadius * 0.18, eps);
    maxRadius = max(minRadius, ceilingRadius - particleRadius * 0.15);
}

void compute_lambda(uint selfIndex) {
    vec3 position = particles[selfIndex].predicted_position.xyz;
    float density = 1.0;
    float restDensitySafe = max(restDensity, eps);
    float radius = max(interactionRadius, eps);
    float radiusSq = radius * radius;
    float radiusInv = 1.0 / radius;
    float sumGradSq = 0.0;
    vec3 gradI = vec3(0.0);
    ivec3 baseCell = compute_cell_coords(position);
    ivec3 minCell = max(baseCell - ivec3(1), ivec3(0));
    ivec3 maxCell = min(baseCell + ivec3(1), ivec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1));
    int particleCount = particles.length();

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                int neighborIndex = cellHeads[flatten_cell_index(ivec3(x, y, z))];
                int guard = 0;

                while (neighborIndex >= 0 && guard < particleCount) {
                    if (neighborIndex != int(selfIndex)) {
                        vec3 delta = position - particles[neighborIndex].predicted_position.xyz;
                        float distanceSq = dot(delta, delta);
                        if (distanceSq < radiusSq) {
                            float distance = sqrt(distanceSq);
                            float q = 1.0 - distance * radiusInv;
                            float q2 = q * q;
                            density += q2 * q;

                            if (distanceSq > epsSq) {
                                vec3 grad = -(3.0 * q2 * radiusInv / restDensitySafe) * (delta * inversesqrt(distanceSq));
                                sumGradSq += dot(grad, grad);
                                gradI += grad;
                            }
                        }
                    }

                    neighborIndex = nextParticle[neighborIndex];
                    ++guard;
                }
            }
        }
    }

    sumGradSq += dot(gradI, gradI);
    float constraint = density / restDensitySafe - 1.0;
    float lambda = -constraint / (sumGradSq + constraintRelaxation);
    particles[selfIndex].solver_data = vec4(lambda, density, constraint, 0.0);
}

void compute_delta_position(uint selfIndex) {
    vec3 position = particles[selfIndex].predicted_position.xyz;
    float lambdaI = particles[selfIndex].solver_data.x;
    float restDensitySafe = max(restDensity, eps);
    float radius = max(interactionRadius, eps);
    float radiusSq = radius * radius;
    float radiusInv = 1.0 / radius;
    float minDistance = particleRadius * 2.0;
    vec3 deltaPosition = vec3(0.0);
    ivec3 baseCell = compute_cell_coords(position);
    ivec3 minCell = max(baseCell - ivec3(1), ivec3(0));
    ivec3 maxCell = min(baseCell + ivec3(1), ivec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1));
    int particleCount = particles.length();

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                int neighborIndex = cellHeads[flatten_cell_index(ivec3(x, y, z))];
                int guard = 0;

                while (neighborIndex >= 0 && guard < particleCount) {
                    if (neighborIndex != int(selfIndex)) {
                        vec3 delta = position - particles[neighborIndex].predicted_position.xyz;
                        float distanceSq = dot(delta, delta);
                        if (distanceSq > epsSq && distanceSq < radiusSq) {
                            float distance = sqrt(distanceSq);
                            float q = 1.0 - distance * radiusInv;
                            float q2 = q * q;
                            vec3 grad = -(3.0 * q2 * radiusInv) * (delta * inversesqrt(distanceSq));
                            float lambdaJ = particles[neighborIndex].solver_data.x;
                            float scorr = -nearPressureStrength * q2 * q2;
                            deltaPosition += (lambdaI + lambdaJ + scorr) * grad;
                            if (distance < minDistance)
                                deltaPosition += (delta / distance) * (minDistance - distance);
                        }
                    }

                    neighborIndex = nextParticle[neighborIndex];
                    ++guard;
                }
            }
        }
    }

    deltaPosition *= separationStrength / restDensitySafe;
    float maxCorrection = particleRadius * 0.75;
    float correctionLength = length(deltaPosition);
    if (correctionLength > maxCorrection)
        deltaPosition *= maxCorrection / correctionLength;

    particles[selfIndex].delta_position = vec4(deltaPosition, 0.0);
}

void solve_boundaries(uint selfIndex) {
    vec3 position = particles[selfIndex].predicted_position.xyz + particles[selfIndex].delta_position.xyz;

    if (simulationMode == 1) {
        vec3 normal;
        float distance;
        float minRadius;
        float maxRadius;
        get_planetary_shell_data(position, normal, distance, minRadius, maxRadius);

        if (distance < minRadius)
            position = planetaryCenter + normal * minRadius;
        else if (distance > maxRadius)
            position = planetaryCenter + normal * maxRadius;

        if (planetaryTerrainEnabled != 0) {
            vec3 clampedOffset = position - planetaryCenter;
            float clampedDistance = length(clampedOffset);
            vec3 clampedNormal = clampedDistance > eps ? clampedOffset / clampedDistance : normal;
            position = planetaryCenter + clampedNormal * clamp(clampedDistance, minRadius, maxRadius);
        }

        particles[selfIndex].predicted_position = vec4(position, particles[selfIndex].predicted_position.w);
        particles[selfIndex].delta_position = vec4(0.0);
        return;
    }

    vec3 wallMin = boundsMin + vec3(particleRadius);
    vec3 wallMax = boundsMax - vec3(particleRadius);

    if (position.x < wallMin.x)
        position.x = wallMin.x;
    else if (position.x > wallMax.x)
        position.x = wallMax.x;

    if (position.y < wallMin.y)
        position.y = wallMin.y;
    else if (position.y > wallMax.y)
        position.y = wallMax.y;

    if (position.z < wallMin.z)
        position.z = wallMin.z;
    else if (position.z > wallMax.z)
        position.z = wallMax.z;

    particles[selfIndex].predicted_position = vec4(position, particles[selfIndex].predicted_position.w);
    particles[selfIndex].delta_position = vec4(0.0);
}

vec3 compute_viscosity(uint selfIndex, vec3 velocity) {
    vec3 position = particles[selfIndex].predicted_position.xyz;
    vec3 viscosity = vec3(0.0);
    float radius = max(interactionRadius, eps);
    float radiusSq = radius * radius;
    float radiusInv = 1.0 / radius;
    float weight = 0.0;
    ivec3 baseCell = compute_cell_coords(position);
    ivec3 minCell = max(baseCell - ivec3(1), ivec3(0));
    ivec3 maxCell = min(baseCell + ivec3(1), ivec3(gridSizeX - 1, gridSizeY - 1, gridSizeZ - 1));
    int particleCount = particles.length();

    for (int z = minCell.z; z <= maxCell.z; ++z) {
        for (int y = minCell.y; y <= maxCell.y; ++y) {
            for (int x = minCell.x; x <= maxCell.x; ++x) {
                int neighborIndex = cellHeads[flatten_cell_index(ivec3(x, y, z))];
                int guard = 0;

                while (neighborIndex >= 0 && guard < particleCount) {
                    if (neighborIndex != int(selfIndex)) {
                        vec3 delta = position - particles[neighborIndex].predicted_position.xyz;
                        float distanceSq = dot(delta, delta);
                        if (distanceSq > epsSq && distanceSq < radiusSq) {
                            float distance = sqrt(distanceSq);
                            float q = 1.0 - distance * radiusInv;
                            float q2 = q * q;
                            vec3 neighborVelocity = (particles[neighborIndex].predicted_position.xyz - particles[neighborIndex].position.xyz) / max(dt, eps);
                            float w = q2 * q;
                            viscosity += (neighborVelocity - velocity) * w;
                            weight += w;
                        }
                    }

                    neighborIndex = nextParticle[neighborIndex];
                    ++guard;
                }
            }
        }
    }

    if (weight > 0.0)
        viscosity /= weight;

    return viscosity;
}

void finalize_particle(uint selfIndex) {
    vec3 previousPosition = particles[selfIndex].position.xyz;
    vec3 previousVelocity = particles[selfIndex].velocity.xyz;
    vec3 position = particles[selfIndex].predicted_position.xyz;
    vec3 velocity = (position - previousPosition) / max(dt, eps);
    velocity += compute_viscosity(selfIndex, velocity) * clamp(viscosityStrength * dt, 0.0, 1.0);
    velocity *= max(0.0, 1.0 - velocityDamping * dt);

    if (simulationMode == 1) {
        vec3 normal;
        float distance;
        float minRadius;
        float maxRadius;
        get_planetary_shell_data(position, normal, distance, minRadius, maxRadius);

        if (distance < minRadius) {
            position = planetaryCenter + normal * minRadius;
            float radialVelocity = dot(velocity, normal);
            if (radialVelocity < 0.0)
                velocity -= normal * radialVelocity * (1.0 + restitution);
        }
        else if (distance > maxRadius) {
            position = planetaryCenter + normal * maxRadius;
            float radialVelocity = dot(velocity, normal);
            if (radialVelocity > 0.0)
                velocity -= normal * radialVelocity * (1.0 + restitution);
        }

        if (planetaryTerrainEnabled != 0) {
            vec3 radialNormal;
            vec3 terrainNormal;
            float floorRadius;
            float ceilingRadius;
            float floorClearance;
            float bottomInfluence;
            get_planetary_flow_data(position, radialNormal, terrainNormal, floorRadius, ceilingRadius, floorClearance, bottomInfluence);
            float basinWeight;
            vec3 basinDirection;
            sample_planetary_basin_direction(radialNormal, basinWeight, basinDirection);
            float oceanFill = sample_planetary_physics_mask_binary(radialNormal);
            if (oceanFill < 0.0)
                oceanFill = planetary_ocean_fill(radialNormal);
            float shellDepth = max(ceilingRadius - floorRadius, particleRadius);
            float groundContact = 1.0 - smoothstep(0.0, particleRadius * 1.5, floorClearance);
            float normalVelocity = dot(velocity, terrainNormal);
            if (normalVelocity < 0.0)
                velocity -= terrainNormal * normalVelocity * mix(0.05, planetaryBottomNormalDamping * 0.82, groundContact);

            vec3 tangentialVelocity = velocity - radialNormal * dot(velocity, radialNormal);
            float tangentialFriction = clamp(groundContact * 0.28, 0.0, 0.28);
            float dryRegion = 1.0 - smoothstep(0.12, 0.78, oceanFill);
            float wetRegion = smoothstep(0.18, 0.82, oceanFill);
            vec3 radialGravity = -radialNormal * planetaryGravityStrength;
            float downslopeDriveStrength = planetaryDownslopeStrength * mix(0.52, 1.0, wetRegion) * mix(0.72, 1.0, bottomInfluence);
            tangentialVelocity += project_onto_plane(radialGravity - terrainNormal * dot(radialGravity, terrainNormal), radialNormal)
                * (downslopeDriveStrength * dt * mix(0.45, 0.95, wetRegion));
            tangentialVelocity += basinDirection
                * (planetaryGravityStrength
                    * dt
                    * basinWeight
                    * planetaryFloodGuidanceStrength
                    * mix(0.018, 0.085, max(groundContact, dryRegion)));

            velocity = tangentialVelocity * mix(1.0, 1.0 - planetaryBottomFriction * 0.82, tangentialFriction)
                + radialNormal * dot(velocity, radialNormal);

            float layerRadius = compute_planetary_surface_layer_radius(floorRadius, ceilingRadius);
            float layerOffset = dot(position - planetaryCenter, radialNormal) - layerRadius;
            float layerInfluence = smoothstep(particleRadius * 0.35, particleRadius * 1.6, shellDepth);
            float layerRetention = layerInfluence * mix(1.0, 0.18, bottomInfluence);
            float clampedLayerOffset = clamp(layerOffset, -particleRadius * 1.45, particleRadius * 1.45);
            velocity -= radialNormal
                * clampedLayerOffset
                * planetaryGravityStrength
                * dt
                * planetarySurfaceLayerAttractionStrength
                * layerRetention
                * 1.65;

            float radialVelocity = dot(velocity, radialNormal);
            float nearLayer = 1.0 - smoothstep(particleRadius * 0.25, particleRadius * 1.8, abs(layerOffset));
            float radialVelocityDamping = clamp(
                0.05 + planetarySurfaceLayerNormalVelocityDamping * mix(0.12, 0.52, nearLayer) * layerRetention,
                0.0,
                0.62);
            velocity -= radialNormal * radialVelocity * radialVelocityDamping;

            float radialComponent = dot(velocity, radialNormal);
            float maxRadialSpeed = shellDepth / max(dt, eps) * mix(0.24, 0.42, oceanFill);
            radialComponent = clamp(radialComponent, -maxRadialSpeed, maxRadialSpeed);
            velocity = project_onto_plane(velocity, radialNormal) + radialNormal * radialComponent;

            float maxTangentialSpeed = mix(
                max(planetaryGravityStrength * 0.2, particleRadius * 18.0),
                max(planetaryGravityStrength * 0.52, particleRadius * 34.0),
                oceanFill);
            vec3 clampedTangentialVelocity = clamp_vector_length(velocity - radialNormal * dot(velocity, radialNormal), maxTangentialSpeed);
            velocity = clampedTangentialVelocity + radialNormal * dot(velocity, radialNormal);
            velocity = clamp_vector_length(velocity, maxTangentialSpeed + shellDepth * 8.0);

            float maxBasinDepth = max(planetaryWaterSurfaceRadius - planetaryRadius, particleRadius);
            particles[selfIndex].solver_data.w = clamp((ceilingRadius - floorRadius) / maxBasinDepth, 0.0, 1.0);
        }
        else
            particles[selfIndex].solver_data.w = 0.0;

        velocity *= collisionDamping;
        particles[selfIndex].position = vec4(position, particles[selfIndex].position.w);
        particles[selfIndex].predicted_position = vec4(position, particles[selfIndex].predicted_position.w);
        particles[selfIndex].velocity = vec4(velocity, particles[selfIndex].velocity.w);
        particles[selfIndex].delta_position = vec4(0.0);
        return;
    }

    vec3 wallMin = boundsMin + vec3(particleRadius);
    vec3 wallMax = boundsMax - vec3(particleRadius);

    if (position.x <= wallMin.x + eps || position.x >= wallMax.x - eps)
        velocity.x *= -restitution;
    if (position.y <= wallMin.y + eps || position.y >= wallMax.y - eps)
        velocity.y *= -restitution;
    if (position.z <= wallMin.z + eps || position.z >= wallMax.z - eps)
        velocity.z *= -restitution;

    velocity *= collisionDamping;
    particles[selfIndex].solver_data.w = 0.0;

    particles[selfIndex].position = vec4(position, particles[selfIndex].position.w);
    particles[selfIndex].predicted_position = vec4(position, particles[selfIndex].predicted_position.w);
    particles[selfIndex].velocity = vec4(velocity, particles[selfIndex].velocity.w);
    particles[selfIndex].delta_position = vec4(0.0);
}

void collect_respawn_candidate(uint selfIndex) {
    if (simulationMode != 1 || planetaryTerrainEnabled == 0)
        return;

    vec3 position = particles[selfIndex].position.xyz;
    vec3 radialNormal;
    vec3 terrainNormal;
    float floorRadius;
    float ceilingRadius;
    float floorClearance;
    float bottomInfluence;
    get_planetary_flow_data(position, radialNormal, terrainNormal, floorRadius, ceilingRadius, floorClearance, bottomInfluence);

    float oceanFill = sample_planetary_physics_mask_binary(radialNormal);
    if (oceanFill < 0.0)
        oceanFill = planetary_ocean_fill(radialNormal);

    float localWaterDepth = max(ceilingRadius - floorRadius, 0.0);
    float radialDistance = length(position - planetaryCenter);
    float overflowDistance = radialDistance - ceilingRadius;
    float floorPenetration = floorRadius - radialDistance;
    bool dryCandidate = oceanFill < 0.08
        || localWaterDepth <= particleRadius * 0.18
        || overflowDistance > particleRadius * 0.85
        || floorPenetration > particleRadius * 0.65;
    if (!dryCandidate)
        return;

    uint slot = atomicAdd(respawnCandidateCount, 1u);
    if (slot < maxRespawnCandidateCount)
        respawnCandidateIndices[slot] = selfIndex;
}

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (passMode == 1) {
        if (i < uint(cellHeads.length()))
            cellHeads[i] = -1;
        return;
    }

    if (i >= uint(particles.length()))
        return;

    if (passMode == 0) {
        if (i == 0u)
            respawnCandidateCount = 0u;
        vec4 debugData;
        float coriolisStrength;
        vec3 velocity = particles[i].velocity.xyz + compute_external_acceleration(particles[i].position.xyz, particles[i].velocity.xyz, debugData, coriolisStrength) * dt;
        vec3 predicted = particles[i].position.xyz + velocity * dt;
        particles[i].predicted_position = vec4(predicted, particles[i].predicted_position.w);
        particles[i].delta_position = vec4(0.0);
        particles[i].solver_data = vec4(0.0);
        particles[i].solver_data.w = coriolisStrength;
        particles[i].debug_data = debugData;
        return;
    }

    if (passMode == 2) {
        int cellIndex = flatten_cell_index(compute_cell_coords(particles[i].predicted_position.xyz));
        nextParticle[i] = atomicExchange(cellHeads[cellIndex], int(i));
        return;
    }

    if (passMode == 3) {
        compute_lambda(i);
        return;
    }

    if (passMode == 4) {
        compute_delta_position(i);
        return;
    }

    if (passMode == 5) {
        solve_boundaries(i);
        return;
    }

    if (passMode == 6) {
        finalize_particle(i);
        collect_respawn_candidate(i);
    }
}
