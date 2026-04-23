#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D particleSurfaceCoverageTexture;
uniform sampler2D particleSurfaceDepthTexture;
uniform sampler2D sceneDepthTexture;
uniform int useSceneDepth;
uniform float particleSurfaceDetailBlend;

vec2 get_texel_size()
{
    return 1.0 / vec2(textureSize(particleSurfaceCoverageTexture, 0));
}

float sample_coverage(vec2 uv)
{
    vec4 coverageSample = texture(particleSurfaceCoverageTexture, uv);
    return clamp(max(max(coverageSample.r, coverageSample.g), coverageSample.b), 0.0, 1.0);
}

float sample_raw_depth(vec2 uv)
{
    return texture(particleSurfaceDepthTexture, uv).r;
}

float sample_depth_validity(vec2 uv)
{
    float depth = sample_raw_depth(uv);
    return (depth > 0.000001 && depth < 0.999999) ? 1.0 : 0.0;
}

float sample_smoothed_coverage(vec2 uv)
{
    vec2 texel = get_texel_size();
    float center = sample_coverage(uv) * 0.28;
    center += sample_coverage(uv + vec2(texel.x, 0.0)) * 0.12;
    center += sample_coverage(uv - vec2(texel.x, 0.0)) * 0.12;
    center += sample_coverage(uv + vec2(0.0, texel.y)) * 0.12;
    center += sample_coverage(uv - vec2(0.0, texel.y)) * 0.12;
    center += sample_coverage(uv + texel) * 0.06;
    center += sample_coverage(uv - texel) * 0.06;
    center += sample_coverage(uv + vec2(texel.x, -texel.y)) * 0.06;
    center += sample_coverage(uv + vec2(-texel.x, texel.y)) * 0.06;
    return clamp(center, 0.0, 1.0);
}

float sample_expanded_coverage(vec2 uv)
{
    vec2 texel = get_texel_size();
    float center_validity = sample_depth_validity(uv);
    float coverage = sample_smoothed_coverage(uv);
    float expansion_scale = mix(0.60, 1.25, particleSurfaceDetailBlend);
    float neighborhood_max = coverage * sample_depth_validity(uv);
    float neighborhood_sum = 0.0;
    float neighborhood_weight = 0.0;

    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texel * expansion_scale;
            float kernel = 1.0 / (1.0 + float(x * x + y * y));
            float validity = sample_depth_validity(uv + offset);
            float sample_value = sample_smoothed_coverage(uv + offset) * validity;
            neighborhood_max = max(neighborhood_max, sample_value);
            neighborhood_sum += sample_value * kernel;
            neighborhood_weight += kernel * validity;
        }
    }

    float neighborhood_avg = neighborhood_weight > 0.000001 ? neighborhood_sum / neighborhood_weight : 0.0;
    float expanded = max(
        coverage,
        max(
            neighborhood_avg * mix(0.54, 0.72, particleSurfaceDetailBlend),
            neighborhood_max * mix(0.34, 0.50, particleSurfaceDetailBlend)));
    float expansion_blend = mix(0.10, 0.42, particleSurfaceDetailBlend) * mix(0.48, 0.88, center_validity);
    return clamp(mix(coverage, expanded, expansion_blend), 0.0, 1.0);
}

float sample_smoothed_depth(vec2 uv)
{
    vec2 texel = get_texel_size();
    float weightedDepth = 0.0;
    float weightedSum = 0.0;
    float frontMostDepth = 1.0;
    float centerDepth = sample_raw_depth(uv);
    float centerCoverage = sample_coverage(uv);

    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texel;
            float coverage = sample_coverage(uv + offset);
            if (coverage <= 0.001)
                continue;

            float depth = texture(particleSurfaceDepthTexture, uv + offset).r;
            if (depth <= 0.000001 || depth >= 0.999999)
                continue;
            float kernel = 1.0 / (1.0 + float(x * x + y * y));
            float depthSimilarity = centerDepth > 0.000001 && centerDepth < 0.999999
                ? 1.0 - smoothstep(mix(0.00018, 0.00008, particleSurfaceDetailBlend), mix(0.0021, 0.0011, particleSurfaceDetailBlend), abs(depth - centerDepth))
                : 1.0;
            float frontBias = 1.0 - smoothstep(0.0, mix(0.0016, 0.00085, particleSurfaceDetailBlend), depth - frontMostDepth);
            float weight = kernel * coverage * max(depthSimilarity, frontBias * 0.68);
            weightedDepth += depth * weight;
            weightedSum += weight;
            frontMostDepth = min(frontMostDepth, depth);
        }
    }

    if (weightedSum <= 0.000001)
        return 1.0;

    float averageDepth = weightedDepth / weightedSum;
    float supportBlend = smoothstep(0.06, 0.32, centerCoverage);
    return mix(frontMostDepth, averageDepth, mix(0.28, 0.78, particleSurfaceDetailBlend) * mix(0.72, 1.0, supportBlend));
}

float sample_surface_confidence(vec2 uv)
{
    float centerCoverage = sample_coverage(uv);
    float centerDepth = sample_smoothed_depth(uv);
    if (centerCoverage <= 0.001 || centerDepth >= 0.999999)
        return 0.0;

    vec2 texel = get_texel_size();
    float support = 0.0;
    float weightTotal = 0.0;

    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 sampleUv = uv + vec2(float(x), float(y)) * texel;
            float kernel = 1.0 / (1.0 + float(x * x + y * y));
            float validity = sample_depth_validity(sampleUv);
            float sampleCoverage = sample_coverage(sampleUv);
            float sampleDepth = sample_raw_depth(sampleUv);
            float coverageSupport = smoothstep(0.035, 0.18, sampleCoverage) * validity;
            float depthSupport = 1.0 - smoothstep(mix(0.00024, 0.00012, particleSurfaceDetailBlend), mix(0.0022, 0.0010, particleSurfaceDetailBlend), abs(sampleDepth - centerDepth));
            support += coverageSupport * depthSupport * kernel;
            weightTotal += kernel;
        }
    }

    return weightTotal > 0.000001 ? clamp((support / weightTotal) * mix(1.95, 2.35, particleSurfaceDetailBlend), 0.0, 1.0) : 0.0;
}

vec3 reconstruct_surface_normal(vec2 uv)
{
    vec2 texel = get_texel_size();
    vec2 fineOffset = texel * mix(1.0, 0.85, particleSurfaceDetailBlend);
    vec2 coarseOffset = texel * mix(2.0, 1.35, particleSurfaceDetailBlend);
    float depthL = mix(sample_smoothed_depth(uv - vec2(coarseOffset.x, 0.0)), sample_smoothed_depth(uv - vec2(fineOffset.x, 0.0)), 0.62);
    float depthR = mix(sample_smoothed_depth(uv + vec2(coarseOffset.x, 0.0)), sample_smoothed_depth(uv + vec2(fineOffset.x, 0.0)), 0.62);
    float depthD = mix(sample_smoothed_depth(uv - vec2(0.0, coarseOffset.y)), sample_smoothed_depth(uv - vec2(0.0, fineOffset.y)), 0.62);
    float depthU = mix(sample_smoothed_depth(uv + vec2(0.0, coarseOffset.y)), sample_smoothed_depth(uv + vec2(0.0, fineOffset.y)), 0.62);

    float dx = depthR - depthL;
    float dy = depthU - depthD;
    float normalScale = mix(1080.0, 760.0, particleSurfaceDetailBlend);
    return normalize(vec3(-dx * normalScale, -dy * normalScale, 1.0));
}

void main()
{
    float coverage = sample_expanded_coverage(TexCoord);
    float confidence = sample_surface_confidence(TexCoord);
    float alpha = smoothstep(mix(0.08, 0.035, particleSurfaceDetailBlend), mix(0.21, 0.15, particleSurfaceDetailBlend), coverage);
    alpha *= smoothstep(mix(0.18, 0.08, particleSurfaceDetailBlend), mix(0.50, 0.22, particleSurfaceDetailBlend), confidence);
    if (alpha <= 0.01)
        discard;

    float particleDepth = sample_smoothed_depth(TexCoord);
    if (particleDepth >= 0.999999)
        discard;

    if (useSceneDepth != 0) {
        float sceneDepth = texture(sceneDepthTexture, TexCoord).r;
        if (particleDepth > sceneDepth + 0.00002)
            discard;

        float waterLead = max(sceneDepth - particleDepth, 0.0);
        float terrainEdgeFade = smoothstep(0.00006, 0.00024, waterLead);
        alpha *= terrainEdgeFade;
        if (alpha <= 0.01)
            discard;
    }

    vec3 normal = reconstruct_surface_normal(TexCoord);
    vec3 lightDir = normalize(vec3(-0.35, 0.45, 0.82));
    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    float diffuse = max(dot(normal, lightDir), 0.0);
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);
    float specular = pow(max(dot(reflect(-lightDir, normal), viewDir), 0.0), 42.0);

    vec3 deepColor = vec3(0.04, 0.14, 0.34);
    vec3 shallowColor = vec3(0.18, 0.58, 0.92);
    vec3 color = mix(deepColor, shallowColor, clamp(coverage * 1.25, 0.0, 1.0));
    color *= 0.42 + diffuse * 0.92;
    color += shallowColor * fresnel * 0.35;
    color += vec3(0.85, 0.95, 1.0) * specular * 0.22;
    float surfaceAlpha = clamp(alpha * (0.76 + confidence * 0.24) + coverage * 0.28, 0.0, 0.97);
    FragColor = vec4(color, surfaceAlpha);
}
