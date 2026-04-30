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
flat in uint WaterRegionId;
in float ShoreDistance;
in float WaveHeight;
in float WaveVelocity;
in float TidalHeight;

out vec4 FragColor;

uniform sampler2D waveStateTexture;
uniform sampler2D tidalHeightTexture;
uniform usampler2D regionIdTexture;
uniform sampler2D shoreDistanceTexture;
uniform int waveStateTextureAvailable;
uniform int tidalHeightTextureAvailable;
uniform int regionIdTextureAvailable;
uniform int shoreDistanceTextureAvailable;
uniform int waveDebugMode;
uniform float debugWaveHeightScale;
uniform float debugWaveVelocityScale;
uniform float debugTidalScale;

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
        return WaterRegionId;

    ivec2 size = textureSize(regionIdTexture, 0);
    ivec2 coord = ivec2(
        clamp(int(floor(fract(uv.x + 1.0) * float(size.x))), 0, size.x - 1),
        clamp(int(floor(clamp(uv.y, 0.0, 1.0) * float(size.y))), 0, size.y - 1));
    return texelFetch(regionIdTexture, coord, 0).r;
}

vec2 sample_wave_state(vec2 uv) {
    if (waveStateTextureAvailable == 0)
        return vec2(0.0);

    return texture(waveStateTexture, vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0))).rg;
}

float sample_tidal_height(vec2 uv) {
    if (tidalHeightTextureAvailable == 0)
        return TidalHeight;

    return texture(tidalHeightTexture, vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0))).r;
}

float sample_shore_distance(vec2 uv) {
    if (shoreDistanceTextureAvailable == 0)
        return ShoreDistance;

    return texture(shoreDistanceTexture, vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0))).r;
}

void main() {
    vec3 surfaceDir = normalize(LocalSurfaceDir);
    vec2 atlasUv = build_planetary_hydrology_uv(surfaceDir);
    vec2 texel = waveStateTextureAvailable != 0
        ? vec2(1.0) / vec2(textureSize(waveStateTexture, 0))
        : vec2(1.0 / 1024.0, 1.0 / 512.0);

    uint regionCenter = WaterRegionId != 0u ? WaterRegionId : sample_region_id(atlasUv);
    vec2 waveCenter = sample_wave_state(atlasUv);
    vec2 waveXP = sample_wave_state(vec2(fract(atlasUv.x + texel.x), clamp(atlasUv.y, 0.0, 1.0)));
    vec2 waveXM = sample_wave_state(vec2(fract(atlasUv.x - texel.x + 1.0), clamp(atlasUv.y, 0.0, 1.0)));
    vec2 waveYP = sample_wave_state(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + texel.y, 0.0, 1.0)));
    vec2 waveYM = sample_wave_state(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - texel.y, 0.0, 1.0)));

    if (sample_region_id(vec2(fract(atlasUv.x + texel.x), clamp(atlasUv.y, 0.0, 1.0))) != regionCenter) waveXP = vec2(0.0);
    if (sample_region_id(vec2(fract(atlasUv.x - texel.x + 1.0), clamp(atlasUv.y, 0.0, 1.0))) != regionCenter) waveXM = vec2(0.0);
    if (sample_region_id(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + texel.y, 0.0, 1.0))) != regionCenter) waveYP = vec2(0.0);
    if (sample_region_id(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - texel.y, 0.0, 1.0))) != regionCenter) waveYM = vec2(0.0);

    float signedHeight = clamp((waveCenter.x + WaveHeight * 0.5) * debugWaveHeightScale, -1.0, 1.0);
    float signedVelocity = clamp((waveCenter.y + WaveVelocity * 0.5) * debugWaveVelocityScale, -1.0, 1.0);
    float tidalAmplitude = clamp(abs(TidalHeight) * 4.0, 0.0, 1.0);
    float amplitude = abs(signedHeight);
    float velocity = abs(signedVelocity);
    float gradient = clamp(length(vec2(waveXP.x - waveXM.x, waveYP.x - waveYM.x)) * debugWaveHeightScale * 0.5, 0.0, 1.0);
    float shore = clamp(sample_shore_distance(atlasUv) * 10.0, 0.0, 1.0);
    float support = clamp(max(ShellSupport, BaseHydrologySupport), 0.0, 1.0);
    float waterPresence = clamp(max(max(ContinuityCoverage, support), smoothstep(0.04, 0.20, WaterLevel01) * smoothstep(0.02, 0.18, WaterColumnDepth01)), 0.0, 1.0);

    if (waveDebugMode == 6) {
        float signedMix = clamp(signedHeight * 0.5 + 0.5, 0.0, 1.0);
        vec3 waveColor = mix(vec3(0.10, 0.35, 1.00), vec3(1.00, 0.20, 0.10), signedMix);
        vec3 baseColor = mix(vec3(0.18, 0.02, 0.24), vec3(0.04, 0.18, 0.34), waterPresence);
        vec3 color = mix(baseColor, waveColor, clamp(max(amplitude, gradient * 0.85), 0.0, 1.0));
        color += vec3(0.10, 0.95, 0.35) * shore * 0.35;

        float majorGrid = max(step(0.975, fract(atlasUv.x * 48.0)), step(0.975, fract(atlasUv.y * 24.0)));
        float minorGrid = max(step(0.992, fract(atlasUv.x * 96.0)), step(0.992, fract(atlasUv.y * 48.0)));
        color = mix(color, vec3(0.95, 0.95, 0.98), majorGrid * 0.85);
        color += vec3(0.45, 0.45, 0.55) * minorGrid * 0.35;

        float alpha = mix(0.26, 0.82, max(max(amplitude, gradient), waterPresence));
        FragColor = vec4(color, alpha);
        return;
    }

    if (waveDebugMode == 7) {
        float tide = clamp(TidalHeight * debugTidalScale, -1.0, 1.0);
        float tideAbs = abs(tide);
        if (waterPresence <= 0.01 && tideAbs <= 0.0001)
            discard;

        vec3 tideColor = tide >= 0.0 ? vec3(1.00, 0.18, 0.98) : vec3(0.06, 0.88, 1.00);
        vec3 baseColor = mix(vec3(0.025, 0.025, 0.035), vec3(0.08, 0.11, 0.14), waterPresence);
        baseColor += vec3(0.16, 0.16, 0.16) * shore * 0.18;
        float tideVisual = smoothstep(0.0005, 0.03, tideAbs);
        float tidePeak = smoothstep(0.18, 0.85, tideAbs);
        float contour = 1.0 - smoothstep(0.40, 0.49, abs(fract(tideAbs * 10.0) - 0.5));
        vec3 color = mix(baseColor, tideColor, tideVisual);
        color += tideColor * tidePeak * 0.28;
        color += vec3(1.0, 1.0, 1.0) * contour * tideVisual * 0.16;
        float alpha = clamp(0.16 + waterPresence * 0.16 + tideVisual * 0.56 + tidePeak * 0.08, 0.0, 0.88);
        FragColor = vec4(color, alpha);
        return;
    }

    if (waveDebugMode == 8) {
        float continuity = clamp(ContinuityCoverage, 0.0, 1.0);
        float shoreline = clamp(ShorelineFade, 0.0, 1.0);
        float column = clamp(WaterColumnDepth01, 0.0, 1.0);
        if (max(max(continuity, column), shoreline) <= 0.01)
            discard;

        vec3 color = vec3(continuity, column, shoreline);
        color = mix(vec3(0.04, 0.04, 0.05), color, 0.96);
        float alpha = clamp(0.24 + max(max(continuity, column), shoreline) * 0.56, 0.0, 0.84);
        FragColor = vec4(color, alpha);
        return;
    }

    if (waveDebugMode == 10) {
        float tideMagnitude = clamp(abs(TidalHeight) * debugTidalScale, 0.0, 1.0);
        if (waterPresence <= 0.01 && tideMagnitude <= 0.0001)
            discard;

        vec3 lowColor = vec3(0.01, 0.01, 0.02);
        vec3 midColor = vec3(0.10, 0.24, 0.95);
        vec3 highColor = vec3(0.70, 0.96, 1.00);
        vec3 peakColor = vec3(1.00, 1.00, 1.00);
        float midBand = smoothstep(0.01, 0.28, tideMagnitude);
        float highBand = smoothstep(0.28, 0.72, tideMagnitude);
        float peakBand = smoothstep(0.72, 1.00, tideMagnitude);
        vec3 color = mix(lowColor, midColor, midBand);
        color = mix(color, highColor, highBand);
        color = mix(color, peakColor, peakBand);
        float contour = 1.0 - smoothstep(0.42, 0.50, abs(fract(tideMagnitude * 12.0) - 0.5));
        color += vec3(1.0, 1.0, 1.0) * contour * smoothstep(0.08, 0.90, tideMagnitude) * 0.20;
        color += vec3(0.20, 0.20, 0.22) * shore * 0.10;
        float alpha = clamp(0.18 + waterPresence * 0.10 + tideMagnitude * 0.74, 0.0, 0.92);
        FragColor = vec4(color, alpha);
        return;
    }

    if (waveDebugMode == 11) {
        float tideCenter = sample_tidal_height(atlasUv);
        float tideXP = sample_tidal_height(vec2(fract(atlasUv.x + texel.x), clamp(atlasUv.y, 0.0, 1.0)));
        float tideXM = sample_tidal_height(vec2(fract(atlasUv.x - texel.x + 1.0), clamp(atlasUv.y, 0.0, 1.0)));
        float tideYP = sample_tidal_height(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + texel.y, 0.0, 1.0)));
        float tideYM = sample_tidal_height(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - texel.y, 0.0, 1.0)));
        if (sample_region_id(vec2(fract(atlasUv.x + texel.x), clamp(atlasUv.y, 0.0, 1.0))) != regionCenter) tideXP = tideCenter;
        if (sample_region_id(vec2(fract(atlasUv.x - texel.x + 1.0), clamp(atlasUv.y, 0.0, 1.0))) != regionCenter) tideXM = tideCenter;
        if (sample_region_id(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y + texel.y, 0.0, 1.0))) != regionCenter) tideYP = tideCenter;
        if (sample_region_id(vec2(fract(atlasUv.x + 1.0), clamp(atlasUv.y - texel.y, 0.0, 1.0))) != regionCenter) tideYM = tideCenter;

        vec2 tideGradient = vec2(tideXP - tideXM, tideYP - tideYM) * debugTidalScale;
        float flowMagnitude = clamp(length(tideGradient) * 8.0, 0.0, 1.0);
        if (waterPresence <= 0.01 && flowMagnitude <= 0.0001)
            discard;

        vec2 flowDir = length(tideGradient) > 0.000001 ? normalize(tideGradient) : vec2(0.0);
        float flowAngle = atan(flowDir.y, flowDir.x);
        vec3 directionColor = 0.5 + 0.5 * cos(vec3(0.0, 2.0943951, 4.1887902) + flowAngle);
        vec3 color = mix(vec3(0.015, 0.015, 0.02), directionColor, smoothstep(0.01, 0.14, flowMagnitude));
        float stripePhase = dot(atlasUv * vec2(96.0, 48.0), flowDir);
        float stripe = 1.0 - smoothstep(0.38, 0.50, abs(fract(stripePhase) - 0.5));
        float arrow = smoothstep(0.58, 0.94, fract(stripePhase * 0.5 + 0.25));
        color += vec3(1.0, 1.0, 1.0) * stripe * flowMagnitude * 0.16;
        color += directionColor * arrow * flowMagnitude * 0.22;
        float alpha = clamp(0.16 + waterPresence * 0.08 + flowMagnitude * 0.76, 0.0, 0.90);
        FragColor = vec4(color, alpha);
        return;
    }

    if (waveDebugMode == 9) {
        float shellLeak = smoothstep(0.12, 0.42, ContinuityCoverage) * (1.0 - smoothstep(0.05, 0.18, WaterColumnDepth01));
        shellLeak = max(shellLeak, smoothstep(0.06, 0.20, abs(TidalHeight)) * (1.0 - smoothstep(0.08, 0.24, ShorelineFade)));
        shellLeak *= max(support, 0.2);
        if (shellLeak <= 0.01 && waterPresence <= 0.01)
            discard;

        vec3 color = mix(vec3(0.24, 0.06, 0.46), vec3(1.0, 0.45, 0.08), smoothstep(0.10, 0.52, shellLeak));
        color = mix(color, vec3(1.0, 0.08, 0.08), smoothstep(0.52, 0.92, shellLeak));
        color = mix(vec3(0.08, 0.03, 0.14), color, clamp(max(shellLeak, waterPresence * 0.6), 0.0, 1.0));
        float alpha = clamp(0.18 + shellLeak * 0.64, 0.0, 0.82);
        FragColor = vec4(color, alpha);
        return;
    }

    vec3 positiveHeightColor = vec3(1.0, 0.15, 0.1);
    vec3 negativeHeightColor = vec3(0.1, 0.35, 1.0);
    vec3 positiveVelocityColor = vec3(1.0, 0.95, 0.15);
    vec3 negativeVelocityColor = vec3(0.15, 1.0, 0.95);

    vec3 color = vec3(0.0);
    color += (signedHeight >= 0.0 ? positiveHeightColor : negativeHeightColor) * amplitude * 0.18;
    color += vec3(0.15, 1.0, 0.2) * gradient * 0.20;
    color += (signedVelocity >= 0.0 ? positiveVelocityColor : negativeVelocityColor) * velocity * 0.08;
    color += vec3(0.85, 0.3, 1.0) * tidalAmplitude * 0.10;
    color += vec3(0.0, 0.18, 0.08) * shore * 0.04;

    float activity = max(gradient, velocity * 0.28);
    float overlaySupport = support * smoothstep(0.04, 0.18, WaterLevel01) * smoothstep(0.02, 0.16, WaterColumnDepth01);
    if (regionCenter == 0u || overlaySupport <= 0.001 || activity <= 0.05)
        discard;

    float alpha = clamp(activity * overlaySupport * 0.045, 0.0, 0.04);
    FragColor = vec4(color, alpha);
}
