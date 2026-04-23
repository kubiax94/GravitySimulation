#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec2 AtlasUv;
in vec3 AtlasData;
in float AtlasFlood;
in float WaterColumnDepth01;
in float ShellSupport;
in float ShorelineFade;
in float BaseHydrologySupport;
in float WaterLevel01;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;
uniform sampler2D waterAtlasTexture;
uniform vec3 planetaryCenterWorld;
uniform float planetarySolidRadiusWorld;
uniform float planetDepthBiasWorld;

float intersect_planet_surface_distance(vec3 rayOrigin, vec3 rayDir, vec3 sphereCenter, float sphereRadius) {
    vec3 oc = rayOrigin - sphereCenter;
    float b = dot(oc, rayDir);
    float c = dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - c;
    if (discriminant <= 0.0)
        return -1.0;

    float sqrtDiscriminant = sqrt(discriminant);
    float nearHit = -b - sqrtDiscriminant;
    if (nearHit > 0.0)
        return nearHit;

    float farHit = -b + sqrtDiscriminant;
    return farHit > 0.0 ? farHit : -1.0;
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float surface_noise(vec3 p) {
    float n = 0.0;
    n += 0.50 * sin(dot(p, vec3(1.13, 1.71, 0.83)));
    n += 0.30 * sin(dot(p, vec3(-1.57, 0.91, 1.29)));
    n += 0.20 * sin(dot(p, vec3(0.74, -1.33, 1.61)));
    return n;
}

vec3 rotate_around_axis(vec3 v, vec3 axis, float angle) {
    vec3 nAxis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    return v * c + cross(nAxis, v) * s + nAxis * dot(nAxis, v) * (1.0 - c);
}

float sample_wave_height(vec3 surfaceDir, float flowAmount) {
    vec3 dir = normalize(surfaceDir);
    vec3 dirA = rotate_around_axis(dir, vec3(0.36, 0.81, -0.46), time * (0.09 + flowAmount * 0.05));
    vec3 dirB = rotate_around_axis(dir, vec3(-0.72, 0.18, 0.67), -time * (0.07 + flowAmount * 0.035));
    vec3 dirC = rotate_around_axis(dir, vec3(0.41, -0.89, 0.19), time * (0.05 + flowAmount * 0.025));
    vec3 domainWarp = vec3(
        surface_noise(dirB * 9.0 + vec3(0.7, -1.1, 0.4)),
        surface_noise(dirC * 11.0 + vec3(-0.5, 0.8, -1.3)),
        surface_noise(dirA * 13.0 + vec3(1.2, 0.3, -0.6))) * 0.085;
    float h = 0.0;
    h += surface_noise((dirA + domainWarp) * 18.0 + vec3(0.9, -0.4, 1.1)) * 0.55;
    h += surface_noise((dirB - domainWarp * 0.7) * 31.0 + vec3(-1.3, 0.8, 0.2)) * 0.30;
    h += surface_noise((dirC + domainWarp * 1.2) * 47.0 + vec3(0.4, 1.6, -0.9)) * 0.15;
    return h * (0.72 + flowAmount * 0.28);
}

float sample_micro_ripple_height(vec3 surfaceDir) {
    vec3 dir = normalize(surfaceDir);
    vec3 dirA = rotate_around_axis(dir, vec3(-0.43, 0.87, 0.24), time * 0.21);
    vec3 dirB = rotate_around_axis(dir, vec3(0.68, 0.29, -0.67), -time * 0.17);
    vec3 dirC = rotate_around_axis(dir, vec3(0.12, -0.94, 0.31), time * 0.13);
    vec3 microWarp = vec3(
        surface_noise(dirB * 23.0 + vec3(0.2, 1.0, -0.6)),
        surface_noise(dirC * 27.0 + vec3(-0.9, 0.3, 1.1)),
        surface_noise(dirA * 19.0 + vec3(1.3, -0.7, 0.5))) * 0.03;
    float h = 0.0;
    h += surface_noise((dirA + microWarp) * 96.0 + vec3(0.4, -1.2, 0.7)) * 0.50;
    h += surface_noise((dirB - microWarp * 0.8) * 148.0 + vec3(-1.0, 0.6, -0.4)) * 0.30;
    h += surface_noise((dirC + microWarp * 1.1) * 212.0 + vec3(1.5, 0.2, -1.1)) * 0.20;
    return h;
}

vec3 build_wave_normal(vec3 baseNormal, vec3 surfaceDir, float flowAmount, float waveStrength) {
    vec3 helperAxis = abs(baseNormal.y) > 0.82 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 tangent = normalize(cross(helperAxis, baseNormal));
    vec3 bitangent = normalize(cross(baseNormal, tangent));

    float sampleOffset = 0.012;
    vec3 dirTPlus = normalize(surfaceDir + tangent * sampleOffset);
    vec3 dirTMinus = normalize(surfaceDir - tangent * sampleOffset);
    vec3 dirBPlus = normalize(surfaceDir + bitangent * sampleOffset);
    vec3 dirBMinus = normalize(surfaceDir - bitangent * sampleOffset);

    float gradT = sample_wave_height(dirTPlus, flowAmount) - sample_wave_height(dirTMinus, flowAmount);
    float gradB = sample_wave_height(dirBPlus, flowAmount) - sample_wave_height(dirBMinus, flowAmount);

    return normalize(baseNormal + tangent * gradT * waveStrength * 2.8 + bitangent * gradB * waveStrength * 2.8);
}

vec4 sample_stabilized_atlas(vec2 uv) {
    vec2 texel = 1.0 / vec2(textureSize(waterAtlasTexture, 0));
    vec4 value = texture(waterAtlasTexture, uv) * 0.28;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0))) * 0.12;
    value += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0))) * 0.12;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.12;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.12;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.06;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.06;
    value += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.06;
    value += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.06;
    return value;
}

vec4 decode_atlas_channels(vec4 atlasSample, vec3 fallbackData, float fallbackFlood) {
    float atlasWeight = max(atlasSample.r, 0.0);
    float occupancy = atlasWeight > 0.00001 ? clamp(atlasWeight, 0.0, 1.0) : clamp(fallbackData.x, 0.0, 1.0);
    float depth01 = atlasWeight > 0.00001 ? clamp(atlasSample.g / atlasWeight, 0.0, 1.0) : clamp(fallbackData.y, 0.0, 1.0);
    float carrier = atlasWeight > 0.00001 ? clamp(atlasSample.b / atlasWeight, 0.0, 1.0) : clamp(fallbackData.z, 0.0, 1.0);
    float flood = atlasWeight > 0.00001 ? clamp(atlasSample.a / atlasWeight, 0.0, 1.0) : clamp(fallbackFlood, 0.0, 1.0);
    return vec4(occupancy, depth01, carrier, flood);
}

vec3 apply_micro_ripple_normal(vec3 baseNormal, vec3 surfaceDir, float detailStrength, float closeViewDetail) {
    vec3 helperAxis = abs(baseNormal.y) > 0.82 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 tangent = normalize(cross(helperAxis, baseNormal));
    vec3 bitangent = normalize(cross(baseNormal, tangent));

    float sampleOffset = 0.0045;
    vec3 dirTPlus = normalize(surfaceDir + tangent * sampleOffset);
    vec3 dirTMinus = normalize(surfaceDir - tangent * sampleOffset);
    vec3 dirBPlus = normalize(surfaceDir + bitangent * sampleOffset);
    vec3 dirBMinus = normalize(surfaceDir - bitangent * sampleOffset);

    float gradT = sample_micro_ripple_height(dirTPlus) - sample_micro_ripple_height(dirTMinus);
    float gradB = sample_micro_ripple_height(dirBPlus) - sample_micro_ripple_height(dirBMinus);

    float rippleStrength = detailStrength * closeViewDetail;
    return normalize(baseNormal + tangent * gradT * rippleStrength * 2.2 + bitangent * gradB * rippleStrength * 2.2);
}

void main() {
    vec4 atlasSample = sample_stabilized_atlas(AtlasUv);
    vec2 atlasTexel = 1.0 / vec2(textureSize(waterAtlasTexture, 0));
    vec4 atlasXP = decode_atlas_channels(sample_stabilized_atlas(vec2(fract(AtlasUv.x + atlasTexel.x), clamp(AtlasUv.y, 0.0, 1.0))), AtlasData, AtlasFlood);
    vec4 atlasXM = decode_atlas_channels(sample_stabilized_atlas(vec2(fract(AtlasUv.x - atlasTexel.x + 1.0), clamp(AtlasUv.y, 0.0, 1.0))), AtlasData, AtlasFlood);
    vec4 atlasYP = decode_atlas_channels(sample_stabilized_atlas(vec2(fract(AtlasUv.x + 1.0), clamp(AtlasUv.y + atlasTexel.y, 0.0, 1.0))), AtlasData, AtlasFlood);
    vec4 atlasYM = decode_atlas_channels(sample_stabilized_atlas(vec2(fract(AtlasUv.x + 1.0), clamp(AtlasUv.y - atlasTexel.y, 0.0, 1.0))), AtlasData, AtlasFlood);
    float atlasWeight = max(atlasSample.r, 0.0);
    float occupancy = max(AtlasData.x, smoothstep(0.010, 0.085, atlasWeight));
    float depth01 = atlasWeight > 0.00001 ? clamp(atlasSample.g / atlasWeight, 0.0, 1.0) : clamp(AtlasData.y, 0.0, 1.0);
    float carrier = atlasWeight > 0.00001 ? clamp(atlasSample.b / atlasWeight, 0.0, 1.0) : clamp(AtlasData.z, 0.0, 1.0);
    float flood = atlasWeight > 0.00001 ? clamp(atlasSample.a / atlasWeight, 0.0, 1.0) : clamp(AtlasFlood, 0.0, 1.0);
    float columnDepth = clamp(WaterColumnDepth01, 0.0, 1.0);
    float support = clamp(ShellSupport, 0.0, 1.0);
    float shoreline = clamp(ShorelineFade, 0.0, 1.0);
    float waterLevel = clamp(WaterLevel01, 0.0, 1.0);
    float hydrologySupport = clamp(BaseHydrologySupport, 0.0, 1.0);
    float oceanGate = smoothstep(0.18, 0.42, waterLevel)
        * smoothstep(0.12, 0.48, columnDepth)
        * smoothstep(0.18, 0.56, shoreline);
    float shorelineSupport = smoothstep(0.20, 0.64, shoreline);
    float deepWaterSupport = smoothstep(0.10, 0.46, columnDepth);
    float stableHydrology = hydrologySupport * shorelineSupport * deepWaterSupport * oceanGate;
    occupancy *= oceanGate;
    carrier *= oceanGate;
    flood *= oceanGate;
    support *= oceanGate;
    float fallbackOccupancy = stableHydrology * 0.12;
    float fallbackCarrier = stableHydrology * (0.06 + 0.10 * columnDepth);
    float fallbackFlood = stableHydrology * (0.06 + 0.10 * columnDepth);
    float fallbackSupport = stableHydrology * (0.06 + 0.08 * shoreline + 0.10 * columnDepth);
    occupancy = max(occupancy, fallbackOccupancy);
    carrier = max(carrier, fallbackCarrier);
    flood = max(flood, fallbackFlood);
    support = max(support, fallbackSupport);
    depth01 = max(depth01, columnDepth * stableHydrology);
    float smoothedOccupancy = clamp((occupancy * 2.0 + atlasXP.x + atlasXM.x + atlasYP.x + atlasYM.x) / 6.0, 0.0, 1.0);
    float smoothedDepth = clamp((depth01 * 2.0 + atlasXP.y + atlasXM.y + atlasYP.y + atlasYM.y) / 6.0, 0.0, 1.0);
    float smoothedFlood = clamp((flood * 2.0 + atlasXP.w + atlasXM.w + atlasYP.w + atlasYM.w) / 6.0, 0.0, 1.0);
    float visualDepth = clamp(mix(columnDepth, smoothedDepth, 0.42), 0.0, 1.0);
    float detailPresence = clamp(max(smoothedOccupancy, stableHydrology), 0.0, 1.0);
    float flowAmount = clamp((carrier * 0.42 + support * 0.26 + smoothedFlood * 0.18 + detailPresence * 0.14) * stableHydrology, 0.0, 1.0);
    vec3 surfaceDir = normalize(FragPos - planetaryCenterWorld);
    float localPhaseA = surface_noise(surfaceDir * 17.0 + vec3(0.4, 1.1, -0.7));
    float localPhaseB = surface_noise(surfaceDir * 23.0 + vec3(-1.2, 0.6, 0.5));
    vec3 motionDirA = rotate_around_axis(surfaceDir, vec3(0.24, 0.91, -0.33), time * 0.11);
    vec3 motionDirB = rotate_around_axis(surfaceDir, vec3(-0.64, 0.17, 0.75), -time * 0.09);
    float proceduralMotionA = 0.5 + 0.5 * sin(time * (0.92 + flowAmount * 0.16) + surface_noise(motionDirA * 29.0 + vec3(0.3, 1.0, -0.8)) * 6.5 + localPhaseA * 2.1);
    float proceduralMotionB = 0.5 + 0.5 * sin(time * (1.14 + flowAmount * 0.12) + surface_noise(motionDirB * 25.0 + vec3(-1.1, 0.5, 0.6)) * 5.8 + localPhaseB * 1.7);
    float proceduralMotion = clamp(proceduralMotionA * 0.58 + proceduralMotionB * 0.42, 0.0, 1.0);
    float surfaceMotion = clamp(max(proceduralMotion * (0.34 + stableHydrology * 0.18), flowAmount * 0.42), 0.0, 1.0);

    float baseAlpha = oceanGate
        * smoothstep(0.10, 0.42, columnDepth)
        * smoothstep(0.16, 0.54, shoreline)
        * smoothstep(0.015, 0.08, stableHydrology);
    float atlasDetailVisibility = clamp(0.94 + detailPresence * 0.03 + carrier * 0.02 + support * 0.01, 0.94, 1.0);
    float alpha = baseAlpha * atlasDetailVisibility;
    if (alpha <= 0.01)
        discard;

    vec3 norm = normalize(Normal);
    float waveStrength = (0.07 + surfaceMotion * 0.18)
        * (0.35 + 0.65 * smoothstep(0.12, 0.72, visualDepth))
        * (0.25 + 0.75 * smoothstep(0.10, 0.42, waterLevel));
    vec3 cameraToFragment = FragPos - viewPos;
    float fragmentDistance = length(cameraToFragment);
    float relativeDistance = fragmentDistance / max(planetarySolidRadiusWorld, 0.0001);
    float closeViewDetail = 1.0 - smoothstep(1.2, 4.0, relativeDistance);
    norm = build_wave_normal(norm, surfaceDir, flowAmount, waveStrength);
    norm = apply_micro_ripple_normal(norm, surfaceDir, (0.010 + surfaceMotion * 0.018) * (0.30 + 0.70 * visualDepth), closeViewDetail);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);
    float viewFacing = max(dot(norm, viewDir), 0.0);
    float diffuse = max(dot(norm, lightDir), 0.0);
    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.4);
    float specular = pow(max(dot(reflect(-lightDir, norm), viewDir), 0.0), mix(56.0, 82.0, surfaceMotion));

    vec3 deepColor = vec3(0.03, 0.12, 0.30);
    vec3 shallowColor = vec3(0.16, 0.52, 0.88);
    vec3 foamTint = vec3(0.42, 0.78, 0.92);
    vec3 color = mix(shallowColor, deepColor, smoothstep(0.08, 0.88, visualDepth));
    float shimmer = 0.5 + 0.5 * sin(time * 2.1 + dot(surfaceDir, normalize(vec3(0.56, 0.61, 0.56))) * 110.0 + surface_noise(surfaceDir * 21.0));
    color = mix(color, foamTint, (1.0 - visualDepth) * surfaceMotion * 0.10);
    color += foamTint * surfaceMotion * shimmer * 0.06;
    color *= lightColor * (0.12 + diffuse * 0.88);
    color += shallowColor * fresnel * (0.18 + surfaceMotion * 0.08);
    color += vec3(0.85, 0.95, 1.0) * specular * (0.12 + surfaceMotion * 0.10);

    float edgeFade = smoothstep(0.08, 0.26, viewFacing);
    vec3 planetaryRadial = normalize(FragPos - planetaryCenterWorld);
    vec3 cameraToPlanet = normalize(viewPos - planetaryCenterWorld);
    float planetSurfaceDistance = intersect_planet_surface_distance(
        viewPos,
        fragmentDistance > 0.000001 ? cameraToFragment / fragmentDistance : vec3(0.0, 0.0, -1.0),
        planetaryCenterWorld,
        planetarySolidRadiusWorld);
    float shellViewGap = planetSurfaceDistance > 0.0 ? max(fragmentDistance - planetSurfaceDistance, 0.0) : planetDepthBiasWorld * 2.0;
    float rimSuppression = 1.0 - smoothstep(0.10, 0.34, viewFacing);
    float depthClamp = smoothstep(planetDepthBiasWorld * 0.8, planetDepthBiasWorld * 2.2, shellViewGap);
    float planetRadiusGap = max(length(FragPos - planetaryCenterWorld) - planetarySolidRadiusWorld, 0.0);
    float radialClamp = 1.0 - smoothstep(planetDepthBiasWorld * 0.45, planetDepthBiasWorld * 1.35, planetRadiusGap);
    float planetaryFacing = max(dot(planetaryRadial, cameraToPlanet), 0.0);
    if (planetaryFacing < 0.52)
        discard;

    float limbFade = smoothstep(0.52, 0.72, planetaryFacing);
    float waterFacingFade = smoothstep(0.10, 0.24, viewFacing);
    float planetClampFade = mix(1.0, depthClamp, rimSuppression) * radialClamp * limbFade * waterFacingFade;
    float surfaceAlpha = clamp(alpha * edgeFade * planetClampFade * (1.02 + columnDepth * 0.24 + shoreline * 0.12), 0.0, 0.96);
    FragColor = vec4(color * intensity, surfaceAlpha);
}
