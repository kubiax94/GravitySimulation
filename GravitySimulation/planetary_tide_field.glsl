#version 460 core
layout(local_size_x = 16, local_size_y = 16) in;

uniform vec2 waveResolution;
uniform float timeSeconds;
uniform float planetaryRadius;
uniform float planetaryShellThickness;
uniform float planetaryWaterSurfaceRadius;
uniform float planetaryTidalStrength;
uniform vec3 planetaryAngularVelocity;
uniform int planetaryExternalGravitySourceCount;
uniform vec4 planetaryExternalGravitySources[8];
uniform sampler2D waterContinuityTexture;
uniform sampler2D waterLevelTexture;
uniform sampler2D waterVetoTexture;
uniform usampler2D regionIdTexture;
uniform sampler2D shoreDistanceTexture;
layout(r32f, binding = 0) writeonly uniform image2D outputTidalHeight;

const float eps = 0.000001;
const float pi = 3.14159265359;
const float two_pi = 6.28318530718;
const float half_pi = 1.57079632679;

ivec2 wrap_coord(ivec2 coord, ivec2 size)
{
    coord.x = (coord.x % size.x + size.x) % size.x;
    coord.y = clamp(coord.y, 0, size.y - 1);
    return coord;
}

vec2 build_uv_from_coord(ivec2 coord, ivec2 size)
{
    return (vec2(coord) + vec2(0.5)) / vec2(size);
}

ivec2 build_texture_coord(vec2 uv, ivec2 size)
{
    return ivec2(
        clamp(int(floor(fract(uv.x + 1.0) * float(size.x))), 0, size.x - 1),
        clamp(int(floor(clamp(uv.y, 0.0, 1.0) * float(size.y))), 0, size.y - 1));
}

float sample_domain_scalar(sampler2D textureSampler, vec2 uv)
{
    ivec2 size = textureSize(textureSampler, 0);
    return texelFetch(textureSampler, build_texture_coord(uv, size), 0).r;
}

uint sample_region(vec2 uv)
{
    ivec2 size = textureSize(regionIdTexture, 0);
    return texelFetch(regionIdTexture, build_texture_coord(uv, size), 0).r;
}

vec3 build_surface_direction(vec2 uv)
{
    float longitude = uv.x * two_pi - pi;
    float latitude = uv.y * pi - half_pi;
    float cosLatitude = cos(latitude);
    return normalize(vec3(cosLatitude * cos(longitude), sin(latitude), cosLatitude * sin(longitude)));
}

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(max(int(waveResolution.x), 1), max(int(waveResolution.y), 1));
    if (coord.x >= size.x || coord.y >= size.y)
        return;

    vec2 uv = build_uv_from_coord(coord, size);
    float continuity = sample_domain_scalar(waterContinuityTexture, uv);
    float waterLevel = sample_domain_scalar(waterLevelTexture, uv);
    float waterVeto = sample_domain_scalar(waterVetoTexture, uv);
    float shorelineDistance = sample_domain_scalar(shoreDistanceTexture, uv);
    uint regionId = sample_region(uv);
    if (continuity <= 0.001 || waterLevel <= 0.001 || regionId == 0u) {
        imageStore(outputTidalHeight, coord, vec4(0.0));
        return;
    }

    vec3 surfaceDir = build_surface_direction(uv);
    vec3 surfacePosition = surfaceDir * max(planetaryWaterSurfaceRadius, planetaryRadius + planetaryShellThickness * 0.5);
    float tidalPotential = 0.0;
    float tidalReference = 0.0;
    for (int sourceIndex = 0; sourceIndex < planetaryExternalGravitySourceCount; ++sourceIndex) {
        vec4 source = planetaryExternalGravitySources[sourceIndex];
        vec3 sourceToSurface = source.xyz - surfacePosition;
        vec3 sourceToCenter = source.xyz;
        float surfaceDistSq = dot(sourceToSurface, sourceToSurface);
        float centerDistSq = dot(sourceToCenter, sourceToCenter);
        if (surfaceDistSq <= eps || centerDistSq <= eps)
            continue;

        float surfaceInvDist = inversesqrt(surfaceDistSq);
        float centerInvDist = inversesqrt(centerDistSq);
        float surfaceRadius = length(surfacePosition);
        vec3 tidalAcceleration = source.w * (
            sourceToSurface * (surfaceInvDist * surfaceInvDist * surfaceInvDist)
            - sourceToCenter * (centerInvDist * centerInvDist * centerInvDist));
        tidalPotential += dot(tidalAcceleration, surfaceDir);
        tidalReference += source.w * surfaceRadius * (centerInvDist * centerInvDist * centerInvDist);
    }

    float shoreAttenuation = smoothstep(max(planetaryShellThickness * 0.05, 0.0001), max(planetaryShellThickness * 0.18, 0.0002), shorelineDistance);
    float continuitySupport = continuity
        * continuity
        * smoothstep(0.10, 0.34, waterLevel)
        * shoreAttenuation;
    float normalizedTide = clamp((tidalPotential / max(tidalReference * 0.35, eps)) * planetaryTidalStrength, -1.0, 1.0);
    float tidalHeight = normalizedTide * continuitySupport;

    imageStore(outputTidalHeight, coord, vec4(tidalHeight, 0.0, 0.0, 0.0));
}
