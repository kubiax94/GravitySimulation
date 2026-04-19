#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;

float wave_noise(vec3 p)
{
    float n = 0.0;
    n += sin(p.x * 3.1 + p.y * 2.7 + p.z * 2.3);
    n += 0.5 * sin(p.x * 6.2 - p.y * 4.1 + p.z * 5.3);
    n += 0.25 * sin(-p.x * 11.4 + p.y * 8.6 + p.z * 9.1);
    return n / 1.75;
}

float fbm_surface(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.55;
    float frequency = 1.0;

    for (int i = 0; i < 4; ++i) {
        value += amplitude * wave_noise(p * frequency);
        frequency *= 1.9;
        amplitude *= 0.5;
        p = p.yzx + vec3(0.37, -0.21, 0.43);
    }

    return value;
}

float flare_tongues(vec3 norm, float time)
{
    float azimuth = atan(norm.z, norm.x);
    float bands_a = max(sin(azimuth * 7.0 + time * 1.25 + norm.y * 4.0), 0.0);
    float bands_b = max(sin(azimuth * 11.0 - time * 1.85 - norm.y * 6.5), 0.0);
    float tongues = pow(max(bands_a, bands_b), 10.0);
    float breakup = 0.5 + 0.5 * fbm_surface(norm * 12.0 + vec3(time * 2.2, -time * 1.4, time));
    return tongues * smoothstep(0.35, 0.92, breakup);
}

float prominence_arcs(vec3 norm, float time)
{
    float azimuth = atan(norm.z, norm.x);
    float arcA = max(sin(azimuth * 4.0 + time * 0.95 + norm.y * 7.5), 0.0);
    float arcB = max(sin(azimuth * 6.5 - time * 1.4 - norm.y * 10.0), 0.0);
    float arcShape = pow(max(arcA, arcB), 14.0);
    float heightMask = smoothstep(0.05, 0.55, abs(norm.y));
    float breakup = 0.5 + 0.5 * fbm_surface(norm * 15.0 + vec3(-time * 1.7, time * 2.1, time * 0.5));
    return arcShape * heightMask * smoothstep(0.45, 0.92, breakup);
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-normalize(FragPos), norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    float pulse = 0.94 + 0.06 * sin(time * 1.9);
    vec3 noisePosA = norm * 4.5 + vec3(0.0, time * 0.75, time * 0.18);
    vec3 noisePosB = norm.zxy * 7.2 + vec3(time * -0.42, time * 0.33, 0.0);
    float surfaceNoiseA = fbm_surface(noisePosA);
    float surfaceNoiseB = fbm_surface(noisePosB);
    float surfaceMix = 0.5 + 0.5 * mix(surfaceNoiseA, surfaceNoiseB, 0.42);
    surfaceMix = smoothstep(0.12, 0.9, surfaceMix);
    surfaceMix = mix(surfaceMix, 0.5 + 0.5 * surfaceNoiseA, 0.18);

    float ndotV = max(dot(norm, viewDir), 0.0);
    float fresnel = pow(1.0 - ndotV, 2.1);
    float rim = smoothstep(0.18, 0.92, fresnel);

    float azimuth = atan(norm.z, norm.x);
    float flareBands = abs(sin(azimuth * 6.0 + time * 1.35));
    flareBands = pow(flareBands, 18.0);
    float flareNoise = 0.5 + 0.5 * fbm_surface(norm * 9.0 + vec3(time * 1.8, -time * 1.1, time * 0.6));
    float tongueFlare = flare_tongues(norm, time);
    float prominence = prominence_arcs(norm, time);
    float solarFlare = rim * flareBands * smoothstep(0.35, 0.95, flareNoise);
    solarFlare = max(solarFlare, tongueFlare * rim);
    float corona = rim * (0.45 + 0.55 * surfaceMix) + solarFlare * 1.65 + prominence * rim * 0.85;

    vec3 coreColor = lightColor * vec3(1.15, 1.0, 0.82);
    vec3 flareColor = lightColor * vec3(1.45, 0.78, 0.28);
    vec3 hotColor = lightColor * vec3(1.8, 1.1, 0.35);

    vec3 baseSurface = mix(coreColor, flareColor, surfaceMix * 0.55);
    vec3 convection = mix(baseSurface, hotColor, solarFlare * 0.55);
    convection = mix(convection, hotColor * 1.05, prominence * 0.35);
    vec3 emissive = convection * intensity * (0.82 + surfaceMix * 0.32) * pulse;
    vec3 arcColor = mix(flareColor, hotColor, 0.75 + 0.25 * surfaceMix) * prominence * rim * (2.1 + 0.35 * pulse);
    vec3 coronaColor = mix(flareColor, hotColor, solarFlare * 0.6) * corona * (1.7 + 0.4 * pulse);
    vec3 specular = spec * lightColor * (0.3 + pulse * 0.25);

    FragColor = vec4(emissive + coronaColor + arcColor + specular, 1.0);
}