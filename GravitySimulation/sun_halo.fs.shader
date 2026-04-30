#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;

float halo_noise(vec3 p)
{
    float n = 0.0;
    n += 0.65 * sin(p.x * 3.4 + p.y * 2.1 + p.z * 2.8);
    n += 0.35 * sin(p.x * 7.2 - p.y * 4.5 + p.z * 5.1);
    return 0.5 + 0.5 * n;
}

float fbm_halo(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.58;
    float frequency = 1.0;

    for (int i = 0; i < 4; ++i) {
        value += amplitude * halo_noise(p * frequency);
        frequency *= 1.95;
        amplitude *= 0.5;
        p = p.yzx + vec3(0.41, -0.27, 0.36);
    }

    return value;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    float ndotV = max(dot(norm, viewDir), 0.0);
    float limb = 1.0 - ndotV;
    float rim = smoothstep(0.08, 0.92, pow(limb, 1.45));
    float outerFade = smoothstep(0.02, 0.88, limb);
    if (outerFade <= 0.001)
        discard;

    float azimuth = atan(norm.z, norm.x);
    float latitude = norm.y;
    float shellNoise = fbm_halo(norm * 6.8 + vec3(time * 0.42, -time * 0.24, time * 0.18));
    float fineNoise = fbm_halo(norm * 12.5 + vec3(-time * 0.65, time * 0.36, time * 0.27));
    float flameBandA = 0.5 + 0.5 * sin(azimuth * 4.5 + time * 0.74 + latitude * 5.2 + shellNoise * 3.1);
    float flameBandB = 0.5 + 0.5 * sin(azimuth * 7.5 - time * 1.02 - latitude * 7.4 + fineNoise * 3.8);
    float flareMask = smoothstep(0.76, 0.96, flameBandA * 0.58 + flameBandB * 0.42);
    float flareBreakup = smoothstep(0.48, 0.92, fineNoise) * smoothstep(0.36, 0.88, shellNoise);
    float prominences = pow(rim, 1.55) * flareMask * flareBreakup;

    float innerBloom = smoothstep(0.04, 0.26, limb) * (0.35 + 0.25 * shellNoise) * (1.0 - flareMask * 0.85);
    float flameCore = smoothstep(0.20, 0.82, rim) * prominences;
    float flameOuter = pow(outerFade, 1.4) * flareMask * (0.35 + 0.65 * shellNoise);
    float coronaMask = smoothstep(0.84, 0.98, shellNoise * 0.55 + fineNoise * 0.45) * flareMask;

    vec3 softColor = lightColor * vec3(1.18, 0.88, 0.42);
    vec3 flareColor = lightColor * vec3(1.65, 0.72, 0.20);
    vec3 hotColor = lightColor * vec3(2.1, 1.08, 0.34);

    vec3 color = softColor * innerBloom * 0.06;
    color += mix(flareColor, hotColor, 0.58 + 0.22 * fineNoise) * flameCore * (0.34 + 0.28 * shellNoise);
    color += mix(flareColor, hotColor, 0.72) * flameOuter * (0.16 + 0.18 * fineNoise);
    color += hotColor * coronaMask * pow(rim, 1.8) * 0.10;

    float alpha = innerBloom * 0.04
        + flameCore * 0.30
        + flameOuter * 0.18
        + coronaMask * 0.08;
    alpha *= flareMask > 0.02 ? 1.0 : 0.0;
    alpha = clamp(alpha, 0.0, 0.34);

    if (dot(color, color) < 0.0005 || alpha <= 0.001)
        discard;

    FragColor = vec4(color * intensity, alpha);
}
