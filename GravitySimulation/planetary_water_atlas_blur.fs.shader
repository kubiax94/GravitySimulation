#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D inputTexture;
uniform int waterDomainTextureAvailable;
uniform sampler2D waterDomainTexture;
uniform vec2 blurDirection;
uniform float blurRadiusScale;

float sample_water_domain(vec2 uv) {
    if (waterDomainTextureAvailable == 0)
        return 1.0;

    vec2 wrapped = vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0));
    return texture(waterDomainTexture, wrapped).r;
}

vec4 sample_atlas(vec2 uv) {
    vec2 wrapped = vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0));
    return texture(inputTexture, wrapped);
}

void accumulate_sample(
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

    vec4 atlasSample = sample_atlas(uv) * sampleMask;
    accumulatedValue += atlasSample * kernelWeight;
    accumulatedWeight += kernelWeight * sampleMask;

    float sampleWeight = max(atlasSample.r, 0.0);
    maxWeight = max(maxWeight, sampleWeight);
    if (sampleWeight > dominantWeight) {
        dominantWeight = sampleWeight;
        dominantNormalized = sampleWeight > 0.00001 ? atlasSample.gba / sampleWeight : vec3(0.0);
    }
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(inputTexture, 0));
    vec2 offset = blurDirection * texel * max(blurRadiusScale, 1.0);
    float centerMask = sample_water_domain(TexCoord);
    if (centerMask <= 0.001) {
        FragColor = vec4(0.0);
        return;
    }

    vec4 value = vec4(0.0);
    float weightSum = 0.0;
    float maxWeight = 0.0;
    float dominantWeight = 0.0;
    vec3 dominantNormalized = vec3(0.0);

    accumulate_sample(TexCoord, 0.227027, value, weightSum, maxWeight, dominantWeight, dominantNormalized);
    accumulate_sample(TexCoord + offset * 1.384615, 0.316216, value, weightSum, maxWeight, dominantWeight, dominantNormalized);
    accumulate_sample(TexCoord - offset * 1.384615, 0.316216, value, weightSum, maxWeight, dominantWeight, dominantNormalized);
    accumulate_sample(TexCoord + offset * 3.230769, 0.070270, value, weightSum, maxWeight, dominantWeight, dominantNormalized);
    accumulate_sample(TexCoord - offset * 3.230769, 0.070270, value, weightSum, maxWeight, dominantWeight, dominantNormalized);

    vec4 blurredAtlas = weightSum > 0.00001 ? value / weightSum : vec4(0.0);
    float blurredWeight = max(blurredAtlas.r, 0.0);
    vec3 normalizedChannels = blurredWeight > 0.00001 ? blurredAtlas.gba / blurredWeight : dominantNormalized;
    float centerWeight = max(sample_atlas(TexCoord).r * centerMask, 0.0);
    float filledWeight = max(blurredWeight, mix(centerWeight, maxWeight, 0.72));
    filledWeight = mix(filledWeight, maxWeight, 0.18);

    FragColor = vec4(filledWeight, normalizedChannels * filledWeight);
}
