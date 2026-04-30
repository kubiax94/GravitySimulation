#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec3 LocalSurfaceDir;
in vec2 AtlasUv;
in vec3 AtlasData;
in float AtlasFlood;
in float WaterColumnDepth01;
in float ShellSupport;
in float ShorelineFade;
in float BaseHydrologySupport;
in float WaterLevel01;
in float ContinuityCoverage;
in float WaterVeto;
in float TerrainOceanMask;
flat in uint WaterRegionId;
in float ShoreDistance;
in float WaveHeight;
in float WaveVelocity;
in float TidalHeight;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;
uniform sampler2D waterAtlasTexture;
uniform int waveStateTextureAvailable;
uniform sampler2D waveStateTexture;
uniform int regionIdTextureAvailable;
uniform usampler2D regionIdTexture;
uniform int shoreDistanceTextureAvailable;
uniform sampler2D shoreDistanceTexture;
uniform int waveDebugMode;
uniform vec3 planetaryCenterWorld;
uniform float planetarySolidRadiusWorld;
uniform float planetaryShellThicknessWorld;
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

vec3 select_tangent_helper_axis(vec3 normal) {
    vec3 absNormal = abs(normal);
    if (absNormal.x <= absNormal.y && absNormal.x <= absNormal.z)
        return vec3(1.0, 0.0, 0.0);
    if (absNormal.y <= absNormal.z)
        return vec3(0.0, 1.0, 0.0);
    return vec3(0.0, 0.0, 1.0);
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
    return h * (0.34 + flowAmount * 0.18);
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
    vec3 helperAxis = select_tangent_helper_axis(baseNormal);
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
    float latitudeAbs = abs(uv.y * 2.0 - 1.0);
    float polarBlurBoost = smoothstep(0.72, 0.96, latitudeAbs);
    float horizontalScale = mix(1.0, 4.0, polarBlurBoost);
    vec4 value = texture(waterAtlasTexture, uv) * 0.24;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x * horizontalScale), clamp(uv.y, 0.0, 1.0))) * 0.10;
    value += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x * horizontalScale + 1.0), clamp(uv.y, 0.0, 1.0))) * 0.10;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.10;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.10;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x * horizontalScale), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.05;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x * horizontalScale), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.05;
    value += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x * horizontalScale + 1.0), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.05;
    value += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x * horizontalScale + 1.0), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.05;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x * 2.0 * horizontalScale), clamp(uv.y, 0.0, 1.0))) * 0.04;
    value += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x * 2.0 * horizontalScale + 1.0), clamp(uv.y, 0.0, 1.0))) * 0.04;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y * 2.0, 0.0, 1.0))) * 0.04;
    value += texture(waterAtlasTexture, vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y * 2.0, 0.0, 1.0))) * 0.04;
    return value;
}

vec4 sample_polar_stable_atlas(vec2 uv, vec3 surfaceDir) {
    vec4 base = sample_stabilized_atlas(uv);
    float polarBlend = smoothstep(0.78, 0.98, abs(surfaceDir.y));
    if (polarBlend <= 0.0)
        return base;

    vec2 texel = 1.0 / vec2(textureSize(waterAtlasTexture, 0));
    vec4 filtered = texture(waterAtlasTexture, uv) * 0.30;
    filtered += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x * 2.0), uv.y)) * 0.18;
    filtered += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x * 2.0 + 1.0), uv.y)) * 0.18;
    filtered += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x * 4.0), uv.y)) * 0.12;
    filtered += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x * 4.0 + 1.0), uv.y)) * 0.12;
    filtered += texture(waterAtlasTexture, vec2(fract(uv.x + texel.x * 8.0), uv.y)) * 0.05;
    filtered += texture(waterAtlasTexture, vec2(fract(uv.x - texel.x * 8.0 + 1.0), uv.y)) * 0.05;
    return mix(base, filtered, polarBlend);
}

vec4 decode_atlas_channels(vec4 atlasSample, vec3 fallbackData, float fallbackFlood) {
    float atlasWeight = max(atlasSample.r, 0.0);
    float occupancy = atlasWeight > 0.00001 ? clamp(atlasWeight, 0.0, 1.0) : clamp(fallbackData.x, 0.0, 1.0);
    float depth01 = atlasWeight > 0.00001 ? clamp(atlasSample.g / atlasWeight, 0.0, 1.0) : clamp(fallbackData.y, 0.0, 1.0);
    float carrier = atlasWeight > 0.00001 ? clamp(atlasSample.b / atlasWeight, 0.0, 1.0) : clamp(fallbackData.z, 0.0, 1.0);
    float flood = atlasWeight > 0.00001 ? clamp(atlasSample.a / atlasWeight, 0.0, 1.0) : clamp(fallbackFlood, 0.0, 1.0);
    return vec4(occupancy, depth01, carrier, flood);
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

uint sample_region_id(vec2 uv) {
    if (regionIdTextureAvailable == 0)
        return 0u;

    ivec2 size = textureSize(regionIdTexture, 0);
    ivec2 coord = ivec2(
        clamp(int(floor(fract(uv.x + 1.0) * float(size.x))), 0, size.x - 1),
        clamp(int(floor(clamp(uv.y, 0.0, 1.0) * float(size.y))), 0, size.y - 1));
    return texelFetch(regionIdTexture, coord, 0).r;
}

float sample_shore_distance(vec2 uv) {
    if (shoreDistanceTextureAvailable == 0)
        return ShoreDistance;

    return texture(shoreDistanceTexture, vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0))).r;
}

vec2 sample_wave_state(vec2 uv) {
    if (waveStateTextureAvailable == 0)
        return vec2(0.0);

    return texture(waveStateTexture, vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0))).rg;
}

vec2 sample_smoothed_wave_state(vec2 uv) {
    if (waveStateTextureAvailable == 0)
        return vec2(0.0);

    vec2 texel = 1.0 / vec2(textureSize(waveStateTexture, 0));
    vec2 value = sample_wave_state(uv) * 0.36;
    value += sample_wave_state(vec2(fract(uv.x + texel.x), clamp(uv.y, 0.0, 1.0))) * 0.12;
    value += sample_wave_state(vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y, 0.0, 1.0))) * 0.12;
    value += sample_wave_state(vec2(fract(uv.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.12;
    value += sample_wave_state(vec2(fract(uv.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.12;
    value += sample_wave_state(vec2(fract(uv.x + texel.x), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.04;
    value += sample_wave_state(vec2(fract(uv.x + texel.x), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.04;
    value += sample_wave_state(vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y + texel.y, 0.0, 1.0))) * 0.04;
    value += sample_wave_state(vec2(fract(uv.x - texel.x + 1.0), clamp(uv.y - texel.y, 0.0, 1.0))) * 0.04;
    return value;
}

vec3 apply_micro_ripple_normal(vec3 baseNormal, vec3 surfaceDir, float detailStrength, float closeViewDetail) {
    vec3 helperAxis = select_tangent_helper_axis(baseNormal);
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
    vec3 surfaceDir = normalize(LocalSurfaceDir);
    vec2 atlasUv = build_planetary_hydrology_uv(surfaceDir);
    vec4 atlasSample = sample_polar_stable_atlas(atlasUv, surfaceDir);
    vec2 atlasTexel = 1.0 / vec2(textureSize(waterAtlasTexture, 0));
    vec4 atlasXP = decode_atlas_channels(sample_polar_stable_atlas(vec2(fract(atlasUv.x + atlasTexel.x), clamp(atlasUv.y, 0.0, 1.0)), surfaceDir), AtlasData, AtlasFlood);
    vec4 atlasXM = decode_atlas_channels(sample_polar_stable_atlas(vec2(fract(atlasUv.x - atlasTexel.x + 1.0), clamp(atlasUv.y, 0.0, 1.0)), surfaceDir), AtlasData, AtlasFlood);
    vec4 atlasYP = decode_atlas_channels(sample_polar_stable_atlas(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + atlasTexel.y, 0.0, 1.0)), surfaceDir), AtlasData, AtlasFlood);
    vec4 atlasYM = decode_atlas_channels(sample_polar_stable_atlas(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - atlasTexel.y, 0.0, 1.0)), surfaceDir), AtlasData, AtlasFlood);
    vec4 atlasFarXP = decode_atlas_channels(sample_polar_stable_atlas(vec2(fract(atlasUv.x + atlasTexel.x * 2.0), clamp(atlasUv.y, 0.0, 1.0)), surfaceDir), AtlasData, AtlasFlood);
    vec4 atlasFarXM = decode_atlas_channels(sample_polar_stable_atlas(vec2(fract(atlasUv.x - atlasTexel.x * 2.0 + 1.0), clamp(atlasUv.y, 0.0, 1.0)), surfaceDir), AtlasData, AtlasFlood);
    vec4 atlasFarYP = decode_atlas_channels(sample_polar_stable_atlas(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + atlasTexel.y * 2.0, 0.0, 1.0)), surfaceDir), AtlasData, AtlasFlood);
    vec4 atlasFarYM = decode_atlas_channels(sample_polar_stable_atlas(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - atlasTexel.y * 2.0, 0.0, 1.0)), surfaceDir), AtlasData, AtlasFlood);
    uint regionCenter = WaterRegionId != 0u ? WaterRegionId : sample_region_id(atlasUv);
    float atlasWeight = max(atlasSample.r, 0.0);
    float occupancy = max(AtlasData.x, smoothstep(0.006, 0.060, atlasWeight));
    float depth01 = atlasWeight > 0.00001 ? clamp(atlasSample.g / atlasWeight, 0.0, 1.0) : clamp(AtlasData.y, 0.0, 1.0);
    float carrier = atlasWeight > 0.00001 ? clamp(atlasSample.b / atlasWeight, 0.0, 1.0) : clamp(AtlasData.z, 0.0, 1.0);
    float flood = atlasWeight > 0.00001 ? clamp(atlasSample.a / atlasWeight, 0.0, 1.0) : clamp(AtlasFlood, 0.0, 1.0);
    float columnDepth = clamp(WaterColumnDepth01, 0.0, 1.0);
    float support = clamp(ShellSupport, 0.0, 1.0);
    float shoreline = clamp(ShorelineFade, 0.0, 1.0);
    float waterLevel = clamp(WaterLevel01, 0.0, 1.0);
    float continuityCoverage = clamp(ContinuityCoverage, 0.0, 1.0);
    float waterVeto = clamp(WaterVeto, 0.0, 1.0);
    float hydrologySupport = clamp(BaseHydrologySupport, 0.0, 1.0);
    float latitudeAbs = abs(surfaceDir.y);
    float polarClosure = smoothstep(0.82, 0.97, latitudeAbs);
    float terrainOceanSupport = smoothstep(0.16, 0.52, clamp(TerrainOceanMask, 0.0, 1.0));
    float oceanGate = smoothstep(0.04, 0.24, waterLevel)
        * smoothstep(0.02, 0.28, columnDepth)
        * smoothstep(0.03, 0.28, shoreline);
    float shorelineSupport = smoothstep(0.06, 0.42, shoreline);
    float deepWaterSupport = smoothstep(0.02, 0.24, columnDepth);
    float stableHydrology = max(hydrologySupport * shorelineSupport * deepWaterSupport * oceanGate,
        continuityCoverage * continuityCoverage * shorelineSupport * deepWaterSupport * 0.24 * smoothstep(0.18, 0.48, waterVeto));
    occupancy *= oceanGate;
    carrier *= oceanGate;
    flood *= oceanGate;
    support = max(support * oceanGate, stableHydrology);
    depth01 = columnDepth;
    float polarContinuousCoverage = clamp(
        smoothstep(0.08, 0.24, waterLevel)
        * smoothstep(0.02, 0.18, columnDepth)
        * mix(smoothstep(0.02, 0.22, shoreline), 1.0, polarClosure),
        0.0,
        1.0);
    occupancy = max(occupancy, polarContinuousCoverage * mix(0.0, 0.85, polarClosure));
    support = max(support, polarContinuousCoverage * mix(0.0, 0.95, polarClosure));
    float smoothedOccupancy = clamp((occupancy * 2.0 + atlasXP.x + atlasXM.x + atlasYP.x + atlasYM.x) / 6.0, 0.0, 1.0);
    float particleOceanSupport = smoothstep(0.10, 0.34, smoothedOccupancy) * deepWaterSupport * smoothstep(0.18, 0.50, waterVeto);
    float landVetoSupport = max(terrainOceanSupport, particleOceanSupport);
    float macroSmoothedOccupancy = clamp((smoothedOccupancy * 2.0 + atlasFarXP.x + atlasFarXM.x + atlasFarYP.x + atlasFarYM.x) / 6.0, 0.0, 1.0);
    float smoothedDepth = columnDepth;
    float smoothedFlood = clamp((flood * 2.0 + atlasXP.w + atlasXM.w + atlasYP.w + atlasYM.w) / 6.0, 0.0, 1.0);
    vec2 waveCenter = sample_wave_state(atlasUv);
    vec2 waveXP = sample_wave_state(vec2(fract(atlasUv.x + atlasTexel.x), clamp(atlasUv.y, 0.0, 1.0)));
    vec2 waveXM = sample_wave_state(vec2(fract(atlasUv.x - atlasTexel.x + 1.0), clamp(atlasUv.y, 0.0, 1.0)));
    vec2 waveYP = sample_wave_state(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + atlasTexel.y, 0.0, 1.0)));
    vec2 waveYM = sample_wave_state(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - atlasTexel.y, 0.0, 1.0)));
    uint regionXP = sample_region_id(vec2(fract(atlasUv.x + atlasTexel.x), clamp(atlasUv.y, 0.0, 1.0)));
    uint regionXM = sample_region_id(vec2(fract(atlasUv.x - atlasTexel.x + 1.0), clamp(atlasUv.y, 0.0, 1.0)));
    uint regionYP = sample_region_id(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + atlasTexel.y, 0.0, 1.0)));
    uint regionYM = sample_region_id(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - atlasTexel.y, 0.0, 1.0)));
    if (regionXP != regionCenter) waveXP = vec2(0.0);
    if (regionXM != regionCenter) waveXM = vec2(0.0);
    if (regionYP != regionCenter) waveYP = vec2(0.0);
    if (regionYM != regionCenter) waveYM = vec2(0.0);
    vec3 cameraToFragment = FragPos - viewPos;
    float fragmentDistance = length(cameraToFragment);
    float relativeDistance = fragmentDistance / max(planetarySolidRadiusWorld, 0.0001);
    float closeViewDetail = 1.0 - smoothstep(1.2, 4.0, relativeDistance);
    float visualDepth = columnDepth;
    float continuousSurfaceCoverage = clamp(max(max(max(stableHydrology, polarContinuousCoverage), continuityCoverage * shorelineSupport * deepWaterSupport * 0.22 * landVetoSupport * smoothstep(0.18, 0.48, waterVeto)), oceanGate * smoothstep(0.03, 0.24, waterLevel) * smoothstep(0.02, 0.24, columnDepth)), 0.0, 1.0);
    float atlasMacroDetail = 0.0;
    float detailPresence = continuousSurfaceCoverage;
    float flowAmount = clamp(continuousSurfaceCoverage * 0.10 * oceanGate, 0.0, 1.0);
    float localShoreDistance = max(sample_shore_distance(atlasUv), ShoreDistance);
    float shoreWaveSupport = clamp(localShoreDistance / max(planetaryShellThicknessWorld * 0.35, 0.0001), 0.0, 1.0);
    float coastlineFeather = smoothstep(0.0, max(planetaryShellThicknessWorld * 0.08, 0.00012), localShoreDistance);
    float coastlineVeto = smoothstep(0.16, 0.48, waterVeto);
    float coastlineDepthPreserve = smoothstep(0.10, 0.34, columnDepth);
    float coastlineEdgeFilter = mix(
        coastlineFeather * coastlineVeto,
        1.0,
        max(max(coastlineDepthPreserve, terrainOceanSupport), smoothstep(0.18, 0.44, landVetoSupport)));
    float atlasCoverageContinuity = macroSmoothedOccupancy
        * smoothstep(0.05, 0.26, columnDepth)
        * smoothstep(0.08, 0.34, shoreWaveSupport);
    continuousSurfaceCoverage = max(continuousSurfaceCoverage, atlasCoverageContinuity * 0.32 * landVetoSupport);
    continuousSurfaceCoverage *= coastlineEdgeFilter;
    detailPresence = continuousSurfaceCoverage;
    float waterBodyConfidence = clamp(
        max(stableHydrology, continuousSurfaceCoverage)
        * smoothstep(0.06, 0.32, columnDepth)
        * smoothstep(0.04, 0.28, localShoreDistance),
        0.0,
        1.0);
    vec2 smoothedWaveCenter = sample_smoothed_wave_state(atlasUv);
    vec2 smoothedWaveXP = sample_smoothed_wave_state(vec2(fract(atlasUv.x + atlasTexel.x), clamp(atlasUv.y, 0.0, 1.0)));
    vec2 smoothedWaveXM = sample_smoothed_wave_state(vec2(fract(atlasUv.x - atlasTexel.x + 1.0), clamp(atlasUv.y, 0.0, 1.0)));
    vec2 smoothedWaveYP = sample_smoothed_wave_state(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + atlasTexel.y, 0.0, 1.0)));
    vec2 smoothedWaveYM = sample_smoothed_wave_state(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - atlasTexel.y, 0.0, 1.0)));
    if (regionXP != regionCenter) smoothedWaveXP = vec2(0.0);
    if (regionXM != regionCenter) smoothedWaveXM = vec2(0.0);
    if (regionYP != regionCenter) smoothedWaveYP = vec2(0.0);
    if (regionYM != regionCenter) smoothedWaveYM = vec2(0.0);
    float waveAmplitude = abs(smoothedWaveCenter.x);
    float waveGradient = length(vec2(smoothedWaveXP.x - smoothedWaveXM.x, smoothedWaveYP.x - smoothedWaveYM.x));
    float tidalMotion = clamp(abs(TidalHeight) * 3.2, 0.0, 1.0);
    float waveDrivenMotion = clamp(abs(WaveHeight) * 4.5 + abs(WaveVelocity) * 0.75 + waveAmplitude * 5.5 + waveGradient * 4.5, 0.0, 1.0);
    float residualProceduralMotion = clamp(flowAmount * 0.015, 0.0, 0.02);
    float polarWaveDamping = mix(1.0, 0.38, polarClosure);
    waveDrivenMotion *= polarWaveDamping;
    float surfaceMotion = clamp(waveDrivenMotion * 0.94 + residualProceduralMotion * 0.06, 0.0, 1.0) * mix(0.68, 1.0, shoreWaveSupport);

    float baseAlpha = smoothstep(0.02, 0.16, continuousSurfaceCoverage)
        * oceanGate
        * smoothstep(0.02, 0.24, columnDepth)
        * smoothstep(0.03, 0.30, shoreline);
    float unsupportedLandWater = (1.0 - landVetoSupport)
        * smoothstep(0.10, 0.34, continuityCoverage)
        * (1.0 - smoothstep(0.06, 0.22, columnDepth))
        * (1.0 - smoothstep(0.16, 0.44, waterVeto));
    float thinLandSpill = (1.0 - terrainOceanSupport)
        * smoothstep(0.18, 0.42, macroSmoothedOccupancy)
        * smoothstep(0.12, 0.30, continuityCoverage)
        * (1.0 - smoothstep(0.06, 0.20, columnDepth))
        * (1.0 - smoothstep(0.08, 0.24, shoreline))
        * (1.0 - smoothstep(0.20, 0.48, waterVeto));
    float alpha = baseAlpha;
    alpha *= (1.0 - unsupportedLandWater) * (1.0 - thinLandSpill);
    alpha *= mix(coastlineEdgeFilter, 1.0, smoothstep(0.18, 0.46, landVetoSupport));
    if (terrainOceanSupport < 0.14
        && waterVeto < 0.18
        && macroSmoothedOccupancy > 0.28
        && continuityCoverage > 0.18
        && columnDepth < 0.14
        && shoreline < 0.18)
        discard;
    if (alpha <= 0.01)
        discard;

    vec3 norm = normalize(Normal);
    float waveStrength = (0.014 + waveDrivenMotion * 0.11 + waveAmplitude * 0.16 + waveGradient * 0.24 + residualProceduralMotion * 0.008)
        * (0.35 + 0.65 * smoothstep(0.12, 0.72, visualDepth))
        * (0.25 + 0.75 * smoothstep(0.10, 0.42, waterLevel));
    norm = build_wave_normal(norm, surfaceDir, flowAmount, waveStrength);
    norm = apply_micro_ripple_normal(norm, surfaceDir, (0.003 + surfaceMotion * 0.005) * (0.18 + 0.42 * visualDepth), closeViewDetail);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);
    float viewFacing = max(dot(norm, viewDir), 0.0);
    float diffuse = max(dot(norm, lightDir), 0.0);
    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.4);
    float specular = pow(max(dot(reflect(-lightDir, norm), viewDir), 0.0), mix(56.0, 82.0, surfaceMotion));
    float broadGlint = pow(max(dot(reflect(-lightDir, norm), viewDir), 0.0), mix(12.0, 20.0, surfaceMotion));
    float sunAlignment = pow(max(dot(viewDir, lightDir), 0.0), 5.0);

    vec3 deepColor = vec3(0.03, 0.12, 0.30);
    vec3 shallowColor = vec3(0.16, 0.52, 0.88);
    vec3 foamTint = vec3(0.42, 0.78, 0.92);
    vec3 color = mix(shallowColor, deepColor, smoothstep(0.08, 0.88, visualDepth));
    float shimmer = 0.72 + 0.28 * sin(time * (1.2 + surfaceMotion * 0.35));
    color = mix(color, foamTint, (1.0 - visualDepth) * waveDrivenMotion * 0.01 * atlasMacroDetail);
    color += foamTint * waveDrivenMotion * shimmer * 0.014;
    color *= lightColor * (0.12 + diffuse * 0.88);
    color += shallowColor * fresnel * (0.12 + waveDrivenMotion * 0.06);
    color += vec3(0.85, 0.95, 1.0) * specular * (0.06 + waveDrivenMotion * 0.06);
    color += vec3(1.0, 0.86, 0.58) * broadGlint * sunAlignment * (0.03 + 0.08 * (1.0 - visualDepth));
    color += vec3(0.02, 0.08, 0.12) * clamp(abs(WaveHeight) * 0.24, 0.0, 0.08);

    float edgeFade = smoothstep(0.02, 0.16, viewFacing);
    vec3 planetaryRadial = normalize(FragPos - planetaryCenterWorld);
    vec3 cameraToPlanet = normalize(viewPos - planetaryCenterWorld);
    float planetSurfaceDistance = intersect_planet_surface_distance(
        viewPos,
        fragmentDistance > 0.000001 ? cameraToFragment / fragmentDistance : vec3(0.0, 0.0, -1.0),
        planetaryCenterWorld,
        planetarySolidRadiusWorld);
    float shellViewGap = planetSurfaceDistance > 0.0 ? max(fragmentDistance - planetSurfaceDistance, 0.0) : planetDepthBiasWorld * 2.0;
    float rimSuppression = 1.0 - smoothstep(0.10, 0.34, viewFacing);
    float shellThicknessWorld = max(planetaryShellThicknessWorld, planetDepthBiasWorld);
    float depthClamp = smoothstep(planetDepthBiasWorld * 0.40, max(planetDepthBiasWorld * 1.4, shellThicknessWorld * 0.72), shellViewGap);
    float planetaryFacing = dot(planetaryRadial, cameraToPlanet);

    float frontHemisphereFade = smoothstep(0.18, 0.42, planetaryFacing);
    float limbFade = smoothstep(0.24, 0.58, planetaryFacing);
    float waterFacingFade = smoothstep(0.02, 0.14, viewFacing);
    float depthClampFloor = mix(0.0, 0.72, waterBodyConfidence) * smoothstep(0.04, 0.22, planetaryFacing);
    float clampBlend = mix(1.0, max(depthClamp, max(0.74, depthClampFloor)), rimSuppression);
    float planetClampFade = clampBlend * max(limbFade, frontHemisphereFade * 0.92) * waterFacingFade;
    float preservedEdgeFade = max(edgeFade, rimSuppression * waterBodyConfidence * 0.38);
    float horizonScatter = pow(1.0 - viewFacing, 2.6) * waterBodyConfidence;
    color += mix(deepColor, shallowColor, 0.55 - visualDepth * 0.18) * horizonScatter * (0.04 + (1.0 - diffuse) * 0.05);
    float surfaceAlpha = clamp(alpha * preservedEdgeFade * planetClampFade * (1.04 + columnDepth * 0.24 + shoreline * 0.12), 0.0, 0.98);
    FragColor = vec4(color * intensity, surfaceAlpha);
}
