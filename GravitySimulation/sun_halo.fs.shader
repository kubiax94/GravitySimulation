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
    float plumeA = max(sin(azimuth * 5.0 + time * 0.74 + latitude * 4.8 + shellNoise * 2.6), 0.0);
    float plumeB = max(sin(azimuth * 8.0 - time * 1.08 - latitude * 6.3 + fineNoise * 3.1), 0.0);
    float prominenceMask = pow(max(plumeA, plumeB), 12.0);
    float prominenceBreakup = smoothstep(0.42, 0.94, fineNoise);
    float prominences = pow(rim, 1.35) * prominenceMask * prominenceBreakup;

    float innerBloom = smoothstep(0.02, 0.34, limb) * (0.55 + 0.45 * shellNoise);
    float midCorona = smoothstep(0.10, 0.72, limb) * (0.35 + 0.65 * fineNoise);
    float outerCorona = pow(outerFade, 1.25) * (0.28 + 0.72 * shellNoise);

    vec3 softColor = lightColor * vec3(1.18, 0.88, 0.42);
    vec3 flareColor = lightColor * vec3(1.65, 0.72, 0.20);
    vec3 hotColor = lightColor * vec3(2.1, 1.08, 0.34);

    vec3 color = softColor * innerBloom * 0.18;
    color += mix(softColor, flareColor, 0.48 + 0.28 * shellNoise) * midCorona * 0.24;
    color += mix(flareColor, hotColor, 0.55 + 0.35 * fineNoise) * outerCorona * 0.14;
    color += mix(flareColor, hotColor, 0.75) * prominences * (0.34 + 0.18 * shellNoise);

    float alpha = innerBloom * 0.12
        + midCorona * 0.16
        + outerCorona * 0.08
        + prominences * 0.22;
    alpha = clamp(alpha, 0.0, 0.42);

    if (dot(color, color) < 0.0005 || alpha <= 0.001)
        discard;

    FragColor = vec4(color * intensity, alpha);
}
