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
    n += sin(dot(p, vec3(2.7, 3.1, 2.3)));
    n += 0.5 * sin(dot(p, vec3(-4.2, 5.4, 6.1)));
    n += 0.25 * sin(dot(p, vec3(8.7, -9.3, 7.9)));
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

float solar_granulation(vec3 norm, float time)
{
    vec3 warp = vec3(
        fbm_surface(norm * 7.0 + vec3(time * 0.08, -time * 0.05, time * 0.03)),
        fbm_surface(norm.zxy * 8.5 + vec3(-time * 0.06, time * 0.07, time * 0.04)),
        fbm_surface(norm.yzx * 9.5 + vec3(time * 0.05, time * 0.04, -time * 0.06))) * 0.18;
    vec3 pA = (norm + warp) * 18.0 + vec3(time * 0.10, -time * 0.06, time * 0.04);
    vec3 pB = (norm.zxy - warp * 0.7) * 30.0 + vec3(-time * 0.14, time * 0.09, time * 0.07);
    float cellA = 0.5 + 0.5 * fbm_surface(pA);
    float cellB = 0.5 + 0.5 * fbm_surface(pB);
    return clamp(cellA * 0.58 + cellB * 0.42, 0.0, 1.0);
}

float solar_intergranular_lanes(vec3 norm, float time)
{
    float granulation = solar_granulation(norm, time);
    float laneMask = 1.0 - smoothstep(0.42, 0.68, granulation);
    float breakup = 0.5 + 0.5 * fbm_surface(norm * 42.0 + vec3(time * 0.22, -time * 0.18, time * 0.13));
    return laneMask * smoothstep(0.34, 0.86, breakup);
}

float solar_active_regions(vec3 norm, float time)
{
    vec3 warp = vec3(
        fbm_surface(norm * 3.2 + vec3(time * 0.07, 0.0, -time * 0.05)),
        fbm_surface(norm.zxy * 4.1 + vec3(-time * 0.04, time * 0.08, 0.0)),
        fbm_surface(norm.yzx * 5.3 + vec3(0.0, time * 0.05, -time * 0.06))) * 0.24;
    float macroA = 0.5 + 0.5 * fbm_surface((norm + warp) * 4.0 + vec3(time * 0.18, time * 0.05, -time * 0.07));
    float macroB = 0.5 + 0.5 * fbm_surface((norm.yzx - warp * 0.6) * 6.5 + vec3(-time * 0.11, time * 0.16, time * 0.08));
    float regions = macroA * 0.62 + macroB * 0.38;
    return smoothstep(0.58, 0.90, regions);
}

float solar_edge_activity(vec3 norm, float time)
{
    float macro = 0.5 + 0.5 * fbm_surface(norm * 5.0 + vec3(time * 0.16, -time * 0.09, time * 0.06));
    float detail = 0.5 + 0.5 * fbm_surface(norm.zxy * 11.0 + vec3(-time * 0.18, time * 0.12, time * 0.08));
    float mask = smoothstep(0.62, 0.90, macro) * smoothstep(0.46, 0.88, detail);
    return mask;
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
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 12.0);

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

    float granulation = solar_granulation(norm, time);
    float lanes = solar_intergranular_lanes(norm, time);
    float activeRegions = solar_active_regions(norm, time);
    float edgeActivity = solar_edge_activity(norm, time);
    float prominence = edgeActivity * rim * (0.35 + 0.65 * activeRegions);
    float solarFlare = rim * edgeActivity * (0.28 + 0.72 * activeRegions);
    float corona = rim * (0.45 + 0.55 * surfaceMix) + solarFlare * 1.65 + prominence * rim * 0.85;

    vec3 deepColor = lightColor * vec3(0.72, 0.28, 0.08);
    vec3 photosphereColor = lightColor * vec3(1.04, 0.76, 0.26);
    vec3 brightCellColor = lightColor * vec3(1.22, 0.94, 0.52);
    vec3 hotColor = lightColor * vec3(1.55, 1.02, 0.34);

    float centerBright = mix(0.88, 1.08, ndotV);
    float photosphere = clamp(granulation * 0.72 + surfaceMix * 0.28, 0.0, 1.0);
    vec3 baseSurface = mix(deepColor, photosphereColor, photosphere);
    baseSurface = mix(baseSurface, brightCellColor, smoothstep(0.52, 0.92, granulation) * 0.72);
    baseSurface = mix(baseSurface, deepColor * 0.58, lanes * 0.82);
    baseSurface = mix(baseSurface, hotColor, activeRegions * 0.42);

    vec3 convection = mix(baseSurface, hotColor, solarFlare * 0.28 + activeRegions * 0.16);
    convection = mix(convection, hotColor * 1.04, prominence * 0.24);
    vec3 emissive = convection * intensity * (0.70 + centerBright * 0.18 + photosphere * 0.14) * pulse;
    vec3 arcColor = mix(photosphereColor, hotColor, 0.76 + 0.24 * surfaceMix) * prominence * rim * (1.65 + 0.22 * pulse);
    vec3 coronaColor = mix(photosphereColor, hotColor, solarFlare * 0.5) * corona * (1.20 + 0.28 * pulse);
    vec3 specular = spec * lightColor * (0.10 + pulse * 0.10);

    FragColor = vec4(emissive + coronaColor + arcColor + specular, 1.0);
}