#version 460 core
layout(local_size_x = 16, local_size_y = 16) in;

uniform vec2 waveResolution;
uniform sampler2D sourceWaveStateTexture;
uniform sampler2D waterDomainTexture;
uniform usampler2D regionIdTexture;
layout(rg16f, binding = 0) writeonly uniform image2D outputWaveState;

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

float sample_domain_scalar(vec2 uv)
{
    ivec2 size = textureSize(waterDomainTexture, 0);
    return texelFetch(waterDomainTexture, build_texture_coord(uv, size), 0).r;
}

uint sample_region(vec2 uv)
{
    ivec2 size = textureSize(regionIdTexture, 0);
    return texelFetch(regionIdTexture, build_texture_coord(uv, size), 0).r;
}

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(max(int(waveResolution.x), 1), max(int(waveResolution.y), 1));
    if (coord.x >= size.x || coord.y >= size.y)
        return;

    vec2 uv = build_uv_from_coord(coord, size);
    float waterDomain = sample_domain_scalar(uv);
    uint regionId = sample_region(uv);
    if (waterDomain <= 0.001 || regionId == 0u) {
        imageStore(outputWaveState, coord, vec4(0.0));
        return;
    }

    vec2 centerState = texelFetch(sourceWaveStateTexture, coord, 0).rg;
    float latitudeAbs = abs(uv.y * 2.0 - 1.0);
    float polarBlend = smoothstep(0.78, 0.98, latitudeAbs);
    int horizontalRadius = max(1, int(round(mix(1.0, 6.0, polarBlend))));
    int verticalRadius = max(1, int(round(mix(1.0, 2.0, polarBlend))));
    float horizontalSigma = mix(0.85, 2.8, polarBlend);
    float verticalSigma = mix(0.80, 1.45, polarBlend);

    vec2 accumulated = vec2(0.0);
    float weightSum = 0.0;

    for (int offsetY = -verticalRadius; offsetY <= verticalRadius; ++offsetY) {
        for (int offsetX = -horizontalRadius; offsetX <= horizontalRadius; ++offsetX) {
            ivec2 sampleCoord = wrap_coord(coord + ivec2(offsetX, offsetY), size);
            vec2 sampleUv = build_uv_from_coord(sampleCoord, size);
            float sampleWaterDomain = sample_domain_scalar(sampleUv);
            if (sampleWaterDomain <= 0.001)
                continue;
            if (sample_region(sampleUv) != regionId)
                continue;

            vec2 sampleState = texelFetch(sourceWaveStateTexture, sampleCoord, 0).rg;
            float normalizedX = float(offsetX) / max(horizontalSigma, 0.0001);
            float normalizedY = float(offsetY) / max(verticalSigma, 0.0001);
            float gaussianWeight = exp(-0.5 * (normalizedX * normalizedX + normalizedY * normalizedY));
            float heightSimilarity = exp(-abs(sampleState.x - centerState.x) * mix(8.0, 12.0, polarBlend));
            float velocitySimilarity = exp(-abs(sampleState.y - centerState.y) * mix(3.5, 5.0, polarBlend));
            float domainSimilarity = 1.0 - smoothstep(0.08, 0.35, abs(sampleWaterDomain - waterDomain));
            float weight = gaussianWeight * heightSimilarity * velocitySimilarity * domainSimilarity;
            if (weight <= 0.0)
                continue;

            accumulated += sampleState * weight;
            weightSum += weight;
        }
    }

    vec2 filtered = weightSum > 0.0 ? accumulated / weightSum : centerState;
    imageStore(outputWaveState, coord, vec4(filtered, 0.0, 0.0));
}
