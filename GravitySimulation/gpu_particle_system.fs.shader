#version 460 core

out vec4 FragColor;

uniform vec3 particleColor;
uniform float particleAlpha;
uniform float particleGlowStrength;
uniform int particleVisualMode;

flat in float ParticleSeed;

float hash11(float p) {
    return fract(sin(p * 91.7) * 43758.5453123);
}

vec3 build_star_tint(float seed) {
    vec3 warm = vec3(1.0, 0.88, 0.76);
    vec3 neutral = vec3(0.96, 0.97, 1.0);
    vec3 cool = vec3(0.72, 0.82, 1.0);
    float tempMix = smoothstep(0.15, 0.85, seed);
    vec3 tint = mix(warm, neutral, smoothstep(0.0, 0.55, tempMix));
    tint = mix(tint, cool, smoothstep(0.55, 1.0, tempMix));
    return tint;
}

void main() {
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radiusSq = dot(centered, centered);
    if (radiusSq > 1.0)
        discard;

    if (particleVisualMode == 1) {
        float radius = sqrt(radiusSq);
        float core = pow(max(1.0 - radius, 0.0), 7.0);
        float halo = exp(-radiusSq * 5.5) * particleGlowStrength;
        float sparkle = smoothstep(0.82, 1.0, hash11(ParticleSeed * 37.0));
        vec3 color = particleColor * build_star_tint(ParticleSeed);
        color *= halo * (0.42 + sparkle * 0.48) + core * (0.95 + sparkle * 0.85);
        float alpha = particleAlpha * clamp(halo * 0.58 + core * 0.92, 0.0, 1.0);
        if (alpha <= 0.01)
            discard;
        FragColor = vec4(color, alpha);
        return;
    }

    if (particleVisualMode == 2) {
        float angle = ParticleSeed * 6.28318530718;
        mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
        vec2 uv = rot * centered;
        float axisScale = mix(0.42, 0.72, hash11(ParticleSeed * 13.0));
        float ellipse = dot(vec2(uv.x / max(axisScale, 0.15), uv.y / max(1.45 - axisScale, 0.22)), vec2(uv.x / max(axisScale, 0.15), uv.y / max(1.45 - axisScale, 0.22)));
        float halo = exp(-ellipse * 2.4) * particleGlowStrength;
        float core = exp(-ellipse * 8.5);
        float arm = 0.5 + 0.5 * sin(atan(uv.y, uv.x) * 2.0 + sqrt(max(ellipse, 0.0)) * 8.0 + ParticleSeed * 11.0);
        vec3 tintA = vec3(0.68, 0.76, 1.0);
        vec3 tintB = vec3(0.96, 0.86, 1.0);
        vec3 color = mix(tintA, tintB, arm * 0.35 + hash11(ParticleSeed * 19.0) * 0.25) * particleColor;
        color *= halo * 0.72 + core * 1.18;
        float alpha = particleAlpha * clamp(halo * 0.42 + core * 0.74, 0.0, 1.0);
        if (alpha <= 0.01)
            discard;
        FragColor = vec4(color, alpha);
        return;
    }

    if (particleVisualMode == 3) {
        float radius = sqrt(radiusSq);
        float angle = ParticleSeed * 6.28318530718;
        mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
        vec2 uv = rot * centered;
        float lobeLength = mix(0.26, 0.52, hash11(ParticleSeed * 5.0));
        float lobeWidth = mix(1.3, 2.6, hash11(ParticleSeed * 9.0));
        float core = exp(-radiusSq * 10.5);
        float corona = exp(-radiusSq * 2.2) * particleGlowStrength;
        float lobe = exp(-((uv.x * uv.x) / max(lobeLength * lobeLength, 0.0001) + (uv.y * uv.y) * lobeWidth));
        float wispNoise = 0.55 + 0.45 * sin(atan(uv.y, uv.x) * 3.0 + radius * 9.0 + ParticleSeed * 21.0);
        float wisps = lobe * pow(max(1.0 - radius, 0.0), 0.65) * wispNoise;
        vec3 warm = vec3(1.0, 0.62, 0.18);
        vec3 hot = vec3(1.0, 0.88, 0.46);
        vec3 color = mix(warm, hot, 0.35 + 0.65 * core) * particleColor;
        color *= corona * 0.72 + core * 1.1 + wisps * 1.4;
        float alpha = particleAlpha * clamp(corona * 0.18 + core * 0.36 + wisps * 0.42, 0.0, 1.0);
        if (alpha <= 0.01)
            discard;
        FragColor = vec4(color, alpha);
        return;
    }

    float glow = 1.0 - radiusSq;
    FragColor = vec4(particleColor * (0.6 + 0.4 * glow), particleAlpha);
}
