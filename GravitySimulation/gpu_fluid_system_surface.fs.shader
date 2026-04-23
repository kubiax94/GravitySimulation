#version 460 core

out vec4 FragColor;

uniform vec3 particleColor;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform mat4 projection;
uniform float particleRadius;
uniform float surfaceOpacity;
uniform int renderPrimitiveMode;
uniform int surfaceInputPass;
uniform int simulationMode;
uniform int useSceneDepth;
uniform sampler2D sceneDepthTexture;
uniform int debugVisualizationMode;
layout(location = 1) out vec4 FrontDepthOutput;

in vec3 CenterViewPos;
in vec3 DebugColorData;
in vec3 SurfaceWorldPos;
in vec3 SurfaceWorldNormal;
in vec3 ParticleCenterWorldPos;
in vec3 PlanetaryCenterWorldPos;
flat in float PlanetarySolidRadiusWorld;
flat in float ParticleRadiusWorld;
in float WaterDepth01;
in float WaterColumnDepth01;
in float WaterSurfaceBand01;
in float SurfaceCarrierWeight;
flat in float RenderFloodMask;
flat in vec2 SurfaceSpriteAxisMajor;
flat in vec2 SurfaceSpriteAxisMinor;
flat in vec2 SurfaceSpriteScale;
flat in float SurfaceCapsuleBlend;

vec3 build_debug_color() {
    vec3 baseColor = particleColor;

    if (debugVisualizationMode == 1) {
        float oceanFill = clamp(DebugColorData.x, 0.0, 1.0);
        return mix(vec3(0.18, 0.24, 0.42), vec3(0.12, 0.58, 1.0), oceanFill);
    }

    if (debugVisualizationMode == 2) {
        float severity = smoothstep(0.12, 0.42, abs(DebugColorData.x));
        float hotspot = smoothstep(0.28, 0.62, abs(DebugColorData.x));
        vec3 warningColor = mix(baseColor, vec3(0.98, 0.78, 0.24), severity * 0.72);
        return mix(warningColor, vec3(1.0, 0.32, 0.18), hotspot * 0.82);
    }

    if (debugVisualizationMode == 3) {
        float centered = clamp(DebugColorData.x * 0.18 + 0.5, 0.0, 1.0);
        return mix(vec3(0.88, 0.62, 0.24), vec3(0.16, 0.56, 0.98), centered);
    }

    if (debugVisualizationMode == 4) {
        float speed = length(DebugColorData);
        if (speed <= 0.0001)
            return baseColor * 0.55;

        vec3 direction = normalize(DebugColorData);
        vec3 absDirection = abs(direction);
        vec3 directionColor;

        if (absDirection.x >= absDirection.y && absDirection.x >= absDirection.z)
            directionColor = direction.x >= 0.0 ? vec3(1.0, 0.32, 0.24) : vec3(0.72, 0.18, 0.72);
        else if (absDirection.y >= absDirection.z)
            directionColor = direction.y >= 0.0 ? vec3(0.24, 1.0, 0.38) : vec3(1.0, 0.82, 0.22);
        else
            directionColor = direction.z >= 0.0 ? vec3(0.22, 0.58, 1.0) : vec3(0.22, 1.0, 1.0);

        float strength = smoothstep(0.02, 1.25, speed);
        return mix(baseColor * 0.45, directionColor, 0.2 + strength * 0.8);
    }

    if (debugVisualizationMode == 5) {
        float clearance = clamp(DebugColorData.x, 0.0, 1.0);
        float highlight = smoothstep(0.015, 0.12, clearance);
        return mix(vec3(0.12, 0.28, 0.85), vec3(1.0, 0.82, 0.22), highlight);
    }

    if (debugVisualizationMode == 6) {
        float depthToSurface = clamp(DebugColorData.x, 0.0, 1.0);
        float nearSurface = 1.0 - smoothstep(0.01, 0.09, depthToSurface);
        return mix(vec3(0.08, 0.18, 0.42), vec3(1.0, 0.38, 0.24), nearSurface);
    }

    if (debugVisualizationMode == 7) {
        float strength = smoothstep(0.0005, 0.08, DebugColorData.x);
        return mix(vec3(0.08, 0.12, 0.22), vec3(0.98, 0.34, 0.18), strength);
    }

    if (debugVisualizationMode == 8) {
        float strength = smoothstep(0.00005, 0.01, DebugColorData.x);
        return mix(vec3(0.06, 0.10, 0.18), vec3(0.28, 0.92, 1.0), strength);
    }

    if (debugVisualizationMode == 9) {
        float speed = length(DebugColorData);
        if (speed <= 0.0001)
            return baseColor * 0.45;

        vec3 direction = normalize(DebugColorData);
        vec3 absDirection = abs(direction);
        vec3 directionColor;
        if (absDirection.x >= absDirection.y && absDirection.x >= absDirection.z)
            directionColor = direction.x >= 0.0 ? vec3(1.0, 0.24, 0.24) : vec3(0.78, 0.18, 0.88);
        else if (absDirection.y >= absDirection.z)
            directionColor = direction.y >= 0.0 ? vec3(0.24, 1.0, 0.34) : vec3(1.0, 0.82, 0.18);
        else
            directionColor = direction.z >= 0.0 ? vec3(0.20, 0.56, 1.0) : vec3(0.18, 1.0, 1.0);

        float strength = smoothstep(0.0005, 0.08, speed);
        return mix(baseColor * 0.38, directionColor, 0.18 + strength * 0.82);
    }

    return baseColor;
}

float sample_scene_depth() {
    if (useSceneDepth == 0)
        return 1.0;

    ivec2 pixel_coord = ivec2(gl_FragCoord.xy);
    ivec2 depth_size = textureSize(sceneDepthTexture, 0);
    if (pixel_coord.x < 0 || pixel_coord.y < 0 || pixel_coord.x >= depth_size.x || pixel_coord.y >= depth_size.y)
        return 1.0;

    return texelFetch(sceneDepthTexture, pixel_coord, 0).r;
}

float normalize_axis_length(vec2 axis) {
    return max(length(axis), 0.000001);
}

float compute_surface_input_metric(vec2 centered) {
    vec2 majorAxis = SurfaceSpriteAxisMajor / normalize_axis_length(SurfaceSpriteAxisMajor);
    vec2 minorAxis = SurfaceSpriteAxisMinor / normalize_axis_length(SurfaceSpriteAxisMinor);
    vec2 aligned = vec2(dot(centered, majorAxis), dot(centered, minorAxis));
    vec2 safeScale = max(SurfaceSpriteScale, vec2(0.52));
    vec2 ellipseCoords = vec2(aligned.x / safeScale.x, aligned.y / safeScale.y);
    float ellipseMetric = dot(ellipseCoords, ellipseCoords);

    float capsuleHalfSpan = max(safeScale.x - safeScale.y, 0.0);
    float capsuleAxis = max(abs(aligned.x) - capsuleHalfSpan, 0.0);
    vec2 capsuleCoords = vec2(capsuleAxis / max(safeScale.y, 0.52), aligned.y / max(safeScale.y, 0.52));
    float capsuleMetric = dot(capsuleCoords, capsuleCoords);
    return mix(ellipseMetric, capsuleMetric, clamp(SurfaceCapsuleBlend, 0.0, 1.0));
}

void main() {
    if (debugVisualizationMode == 1 && RenderFloodMask < 0.5)
        discard;

    if (renderPrimitiveMode != 0) {
        const float depth_bias = 0.00002;
        const float scene_depth = sample_scene_depth();
        if (gl_FragCoord.z > scene_depth + depth_bias)
            discard;

        vec3 normal = normalize(SurfaceWorldNormal);
        vec3 planetaryNormal = normal;
        if (simulationMode == 1) {
            planetaryNormal = normalize(ParticleCenterWorldPos - PlanetaryCenterWorldPos);
            normal = planetaryNormal;
        }

        vec3 viewDir = normalize(viewPos - SurfaceWorldPos);
        vec3 lightDir = normalize(lightPos - SurfaceWorldPos);
        float planetaryLight = max(dot(planetaryNormal, lightDir), 0.0);
        float diffuse = max(dot(normal, lightDir), 0.0);
        if (simulationMode == 1)
            diffuse = planetaryLight;

        float spec = pow(max(dot(viewDir, reflect(-lightDir, normal)), 0.0), 56.0);
        if (simulationMode == 1)
            spec *= smoothstep(0.08, 0.22, planetaryLight);

        float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.6);

        vec3 baseColor = build_debug_color();
        vec3 deepTint = simulationMode == 1
            ? baseColor * vec3(0.14, 0.24, 0.56)
            : baseColor * vec3(0.22, 0.34, 0.48);
        vec3 shallowTint = simulationMode == 1
            ? mix(vec3(0.24, 0.82, 0.88), baseColor * vec3(0.88, 1.02, 1.08), 0.5)
            : mix(baseColor, vec3(0.82, 0.94, 1.0), 0.22);
        vec3 waterBase = simulationMode == 1
            ? mix(shallowTint, deepTint, smoothstep(0.08, 0.78, WaterDepth01))
            : mix(deepTint, shallowTint, clamp(0.16 + diffuse * 0.34, 0.0, 1.0));
        vec3 color;
        if (simulationMode == 1) {
            float shoreline = 1.0 - smoothstep(0.10, 0.28, WaterDepth01);
            color = baseColor * 0.16 + waterBase * lightColor * (0.12 + diffuse * 0.72);
            color += shallowTint * shoreline * 0.12;
            color += lightColor * spec * 0.10;
            color += shallowTint * fresnel * 0.04;
        }
        else {
            color = waterBase * lightColor * (0.18 + diffuse * 0.95);
            color += lightColor * spec * 0.42;
            color += shallowTint * lightColor * fresnel * 0.18;
        }

        float alpha = surfaceOpacity;
        if (simulationMode == 1) {
            float depthOpacity = mix(0.34, 0.92, smoothstep(0.03, 0.65, WaterDepth01));
            float fresnelOpacity = fresnel * 0.08;
            alpha = clamp(surfaceOpacity * depthOpacity + fresnelOpacity, 0.22, 0.96);
        }

        FragColor = vec4(color, alpha);
        return;
    }

    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radiusSq = dot(centered, centered);
    float surfaceMetric = radiusSq;
    if (simulationMode == 1 && surfaceInputPass != 0)
        surfaceMetric = compute_surface_input_metric(centered);

    if (surfaceMetric > 1.0)
        discard;

    const float scene_depth = sample_scene_depth();
    float zMetric = simulationMode == 1 && surfaceInputPass != 0 ? surfaceMetric : radiusSq;
    float z = sqrt(max(1.0 - min(zMetric, 1.0), 0.0));
    float depthScale = simulationMode == 1 && surfaceInputPass != 0
        ? mix(0.015, 0.045, clamp(SurfaceCarrierWeight, 0.0, 1.0))
        : 1.0;
    vec3 sphereViewPos = CenterViewPos + vec3(centered * particleRadius, z * particleRadius * depthScale);
    vec4 clipPos = projection * vec4(sphereViewPos, 1.0);
    float ndcDepth = clipPos.z / clipPos.w;
    gl_FragDepth = ndcDepth * 0.5 + 0.5;
    if (gl_FragDepth > scene_depth + 0.00002)
        discard;

    float thickness = pow(max(1.0 - radiusSq, 0.0), simulationMode == 1 ? 0.42 : 1.35);
    float alpha = clamp(thickness * surfaceOpacity, 0.0, 1.0);
    vec3 color = build_debug_color() * (0.5 + 0.5 * z);
    if (simulationMode == 1 && surfaceInputPass != 0) {
        float floodMask = clamp(RenderFloodMask, 0.0, 1.0);
        float columnDepth = clamp(WaterColumnDepth01, 0.0, 1.0);
        float surfaceBand = clamp(WaterSurfaceBand01, 0.0, 1.0);
        float carrierWeight = clamp(SurfaceCarrierWeight, 0.0, 1.0);
        if (carrierWeight < 0.01)
            discard;
        float splat = pow(max(1.0 - surfaceMetric, 0.0), 0.82);
        float core = smoothstep(1.0, 0.0, surfaceMetric * 0.94);
        float shellSuppression = 1.0 - smoothstep(0.18, 0.62, clamp(WaterDepth01, 0.0, 1.0));
        float coverage = clamp(max(splat * 0.72, core) * carrierWeight * shellSuppression, 0.0, 1.0);
        if (coverage < 0.01)
            discard;
        FrontDepthOutput = vec4(gl_FragDepth, 0.0, 0.0, 1.0);
        FragColor = vec4(vec3(coverage), coverage);
        return;
    }

    if (simulationMode == 1) {
        float shoreline = 1.0 - smoothstep(0.10, 0.28, WaterDepth01);
        vec3 baseColor = build_debug_color();
        vec3 deepTint = baseColor * vec3(0.14, 0.24, 0.56);
        vec3 shallowTint = mix(vec3(0.24, 0.82, 0.88), baseColor * vec3(0.88, 1.02, 1.08), 0.5);
        vec3 waterBase = mix(shallowTint, deepTint, smoothstep(0.08, 0.78, WaterDepth01));
        float edgeSoftness = smoothstep(1.0, 0.12, radiusSq);
        color = waterBase * (0.72 + 0.28 * edgeSoftness);
        color += shallowTint * shoreline * 0.08 * edgeSoftness;
        alpha = clamp(surfaceOpacity * (0.16 + thickness * 0.95), 0.12, 0.9);
    }

    if (alpha < 0.32)
        discard;

    FrontDepthOutput = vec4(0.0, 0.0, 0.0, 1.0);
    FragColor = vec4(color, 1.0);
}
