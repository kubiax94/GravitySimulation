#version 460 core
layout(local_size_x = 16, local_size_y = 16) in;

uniform float dt;
uniform float timeSeconds;
uniform float propagationSpeed;
uniform float forcingScale;
uniform float openWaterDamping;
uniform float shoreDamping;
uniform float shoreTransitionDistance;
uniform vec2 waveResolution;
uniform sampler2D waveStateTexture;
uniform sampler2D supportAtlasTexture;
uniform sampler2D forcingTexture;
uniform sampler2D waterDomainTexture;
uniform sampler2D waterLevelTexture;
uniform sampler2D tidalHeightTexture;
uniform sampler2D waterVetoTexture;
uniform usampler2D regionIdTexture;
uniform sampler2D shoreDistanceTexture;
layout(rg16f, binding = 0) writeonly uniform image2D outputWaveState;

ivec2 wrap_coord(ivec2 coord, ivec2 size) {
    coord.x = (coord.x % size.x + size.x) % size.x;
    coord.y = clamp(coord.y, 0, size.y - 1);
    return coord;
}

vec2 build_uv_from_coord(ivec2 coord, ivec2 size) {
    return (vec2(coord) + vec2(0.5)) / vec2(size);
}

ivec2 build_texture_coord(vec2 uv, ivec2 size) {
    return ivec2(
        clamp(int(floor(fract(uv.x + 1.0) * float(size.x))), 0, size.x - 1),
        clamp(int(floor(clamp(uv.y, 0.0, 1.0) * float(size.y))), 0, size.y - 1));
}

float sample_domain_scalar(sampler2D textureSampler, vec2 uv) {
    ivec2 size = textureSize(textureSampler, 0);
    return texelFetch(textureSampler, build_texture_coord(uv, size), 0).r;
}

uint sample_domain_region(vec2 uv) {
    ivec2 size = textureSize(regionIdTexture, 0);
    return texelFetch(regionIdTexture, build_texture_coord(uv, size), 0).r;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(max(int(waveResolution.x), 1), max(int(waveResolution.y), 1));
    if (coord.x >= size.x || coord.y >= size.y)
        return;

    vec2 uv = build_uv_from_coord(coord, size);
    float waterDomain = sample_domain_scalar(waterDomainTexture, uv);
    uint regionId = sample_domain_region(uv);
    vec2 currentState = texelFetch(waveStateTexture, coord, 0).rg;
    if (waterDomain <= 0.001 || regionId == 0u) {
        imageStore(outputWaveState, coord, vec4(0.0));
        return;
    }

    float centerHeight = currentState.r;
    float centerVelocity = currentState.g;
    vec4 supportAtlas = texelFetch(supportAtlasTexture, coord, 0);
    float atlasWeight = max(supportAtlas.r, 0.0);
    float occupancy = smoothstep(0.0012, 0.020, atlasWeight);
    float depth01 = atlasWeight > 0.00001 ? clamp(supportAtlas.g / atlasWeight, 0.0, 1.0) : 0.0;
    float carrier = atlasWeight > 0.00001 ? clamp(supportAtlas.b / atlasWeight, 0.0, 1.0) : 0.0;
    float flood = atlasWeight > 0.00001 ? clamp(supportAtlas.a / atlasWeight, 0.0, 1.0) : 0.0;
    float waterLevel = sample_domain_scalar(waterLevelTexture, uv);
    float tidalHeight = sample_domain_scalar(tidalHeightTexture, uv);
    float waterVeto = sample_domain_scalar(waterVetoTexture, uv);
    float shorelineDistance = sample_domain_scalar(shoreDistanceTexture, uv);
    vec4 forcingSample = texture(forcingTexture, vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0)));
    if (waterVeto <= 0.001) {
        imageStore(outputWaveState, coord, vec4(0.0));
        return;
    }

    float neighborSum = 0.0;
    float neighborWeight = 0.0;
    const ivec2 offsets[4] = ivec2[4](
        ivec2(1, 0),
        ivec2(-1, 0),
        ivec2(0, 1),
        ivec2(0, -1));

    for (int i = 0; i < 4; ++i) {
        ivec2 neighborCoord = wrap_coord(coord + offsets[i], size);
        vec2 neighborUv = build_uv_from_coord(neighborCoord, size);
        float neighborWater = sample_domain_scalar(waterDomainTexture, neighborUv);
        uint neighborRegionId = sample_domain_region(neighborUv);
        if (neighborWater <= 0.001 || neighborRegionId != regionId)
            continue;

        float neighborTidalHeight = sample_domain_scalar(tidalHeightTexture, neighborUv);
        neighborSum += texelFetch(waveStateTexture, neighborCoord, 0).r + neighborTidalHeight;
        neighborWeight += 1.0;
    }

    float centerResidual = centerHeight;
    float centerSurfaceHeight = centerResidual + tidalHeight;
    float laplacian = neighborWeight > 0.0 ? (neighborSum / neighborWeight) - centerSurfaceHeight : -centerSurfaceHeight;
    float support = occupancy * (0.55 + 0.45 * depth01) * (0.50 + 0.50 * flood) * waterVeto;
    float travelingWaveA = sin((uv.x * 17.0 + uv.y * 9.0) - timeSeconds * 1.8);
    float travelingWaveB = sin((uv.x * -11.0 + uv.y * 13.0) + timeSeconds * 1.2);
    float animatedForcing = (travelingWaveA * 0.65 + travelingWaveB * 0.35);
    float forcingWeight = max(forcingSample.a, 0.0);
    float solverForcing = forcingWeight > 0.00001 ? clamp(forcingSample.r / forcingWeight, 0.0, 1.0) : 0.0;
    float solverCarrier = forcingWeight > 0.00001 ? clamp(forcingSample.g / forcingWeight, 0.0, 1.0) : 0.0;
    float forcing = ((max(support, 0.0) * 0.16 + max(carrier, 0.0) * 0.18 + max(solverCarrier, 0.0) * 0.26) * max(waterLevel, 0.0)
        + solverForcing + animatedForcing * 0.10 * max(support, waterLevel)) * forcingScale;
    forcing *= smoothstep(0.14, 0.42, waterVeto);
    float latitudeAbs = abs(uv.y * 2.0 - 1.0);
    float polarWaveDamping = mix(1.0, 0.34, smoothstep(0.80, 0.97, latitudeAbs));
    forcing *= polarWaveDamping;
    float shoreBlend = sqrt(clamp(shorelineDistance / max(shoreTransitionDistance, 0.0001), 0.0, 1.0));
    float damping = mix(shoreDamping, openWaterDamping, shoreBlend);
    damping = mix(damping * 2.6, damping, smoothstep(0.12, 0.40, waterVeto));
    damping = mix(damping, damping * 1.65, smoothstep(0.80, 0.97, latitudeAbs));
    float nextVelocity = centerVelocity + (laplacian * propagationSpeed * propagationSpeed + forcing - damping * centerVelocity) * dt;
    float nextHeight = centerResidual + nextVelocity * dt;
    nextHeight *= mix(0.72, 0.985, shoreBlend) * polarWaveDamping;
    nextVelocity *= mix(0.76, 0.989, shoreBlend) * mix(1.0, 0.72, smoothstep(0.80, 0.97, latitudeAbs));
    nextHeight = clamp(nextHeight, -1.1, 1.1);
    nextVelocity = clamp(nextVelocity, -2.0, 2.0);

    imageStore(outputWaveState, coord, vec4(nextHeight, nextVelocity, 0.0, 0.0));
}
