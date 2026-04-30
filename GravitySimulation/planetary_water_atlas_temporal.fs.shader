#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D currentAtlasTexture;
uniform sampler2D historyAtlasTexture;
uniform int waterDomainTextureAvailable;
uniform sampler2D waterDomainTexture;
uniform float historyBlend;

vec4 sample_atlas(sampler2D atlasTexture, vec2 uv) {
    vec2 wrapped = vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0));
    return texture(atlasTexture, wrapped);
}

float sample_water_domain(vec2 uv) {
    if (waterDomainTextureAvailable == 0)
        return 1.0;

    vec2 wrapped = vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0));
    return texture(waterDomainTexture, wrapped).r;
}

void accumulate_temporal_sample(
    sampler2D atlasTexture,
    vec2 uv,
    float kernelWeight,
    inout vec4 accumulatedValue,
    inout float accumulatedWeight,
    inout float maxWeight,
    inout float dominantWeight,
    inout vec3 dominantNormalized) {
    float sampleMask = sample_water_domain(uv);
    if (sampleMask <= 0.0)
        return;

    vec4 atlasSample = sample_atlas(atlasTexture, uv) * sampleMask;
    accumulatedValue += atlasSample * kernelWeight;
    accumulatedWeight += kernelWeight * sampleMask;

    float sampleWeight = max(atlasSample.r, 0.0);
    maxWeight = max(maxWeight, sampleWeight);
    if (sampleWeight > dominantWeight) {
        dominantWeight = sampleWeight;
        dominantNormalized = sampleWeight > 0.00001 ? atlasSample.gba / sampleWeight : vec3(0.0);
    }
}

vec4 sample_expanded_atlas(sampler2D atlasTexture, vec2 uv) {
    vec2 texel = 1.0 / vec2(textureSize(atlasTexture, 0));
    vec4 accumulatedValue = vec4(0.0);
    float accumulatedWeight = 0.0;
    float maxWeight = 0.0;
    float dominantWeight = 0.0;
    vec3 dominantNormalized = vec3(0.0);

    accumulate_temporal_sample(atlasTexture, uv, 0.28, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);
    accumulate_temporal_sample(atlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0)), 0.18, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);
    accumulate_temporal_sample(atlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0)), 0.18, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);
    accumulate_temporal_sample(atlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.18, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);
    accumulate_temporal_sample(atlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.18, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);
    accumulate_temporal_sample(atlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y + texel.y, 0.0, 1.0)), 0.09, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);
    accumulate_temporal_sample(atlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y - texel.y, 0.0, 1.0)), 0.09, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);
    accumulate_temporal_sample(atlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0)), 0.09, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);
    accumulate_temporal_sample(atlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0)), 0.09, accumulatedValue, accumulatedWeight, maxWeight, dominantWeight, dominantNormalized);

    vec4 filteredAtlas = accumulatedWeight > 0.00001 ? accumulatedValue / accumulatedWeight : vec4(0.0);
    float filteredWeight = max(filteredAtlas.r, 0.0);
    vec3 normalizedChannels = filteredWeight > 0.00001 ? filteredAtlas.gba / filteredWeight : dominantNormalized;
    float expandedWeight = max(filteredWeight, maxWeight * 0.90);
    return vec4(expandedWeight, normalizedChannels * expandedWeight);
}

void main() {
    float centerMask = sample_water_domain(TexCoord);
    if (centerMask <= 0.001) {
        FragColor = vec4(0.0);
        return;
    }

    vec4 currentAtlas = sample_expanded_atlas(currentAtlasTexture, TexCoord);
    vec4 historyAtlas = sample_expanded_atlas(historyAtlasTexture, TexCoord);

    float currentWeight = max(currentAtlas.r, 0.0);
    float historyWeight = max(historyAtlas.r, 0.0);
    float currentPresence = smoothstep(0.0025, 0.028, currentWeight);
    float historyPresence = smoothstep(0.004, 0.038, historyWeight);
    vec4 currentNormalized = currentWeight > 0.00001 ? currentAtlas / currentWeight : vec4(0.0);
    vec4 historyNormalized = historyWeight > 0.00001 ? historyAtlas / historyWeight : vec4(0.0);
    float weightMismatch = abs(currentWeight - historyWeight) / max(max(currentWeight, historyWeight), 0.0001);
    float channelMismatch = clamp(length(currentNormalized.gba - historyNormalized.gba) * 0.6, 0.0, 1.0);
    float mismatch = clamp(max(weightMismatch, channelMismatch), 0.0, 1.0);
    float stableBlend = mix(0.34, 0.88, clamp(historyBlend, 0.0, 1.0));
    float historyRetain = mix(0.64, 0.97, (1.0 - mismatch) * historyPresence);
    vec4 retainedHistory = historyAtlas * historyRetain;
    float historyContribution = stableBlend
        * historyPresence
        * (1.0 - currentPresence * 0.72)
        * (1.0 - mismatch);
    vec4 blendedAtlas = mix(currentAtlas, retainedHistory, historyContribution);
    blendedAtlas.r = max(currentAtlas.r, blendedAtlas.r);

    FragColor = blendedAtlas * centerMask;
}
