#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;

float atmosphere_noise(vec3 p)
{
    float n = 0.0;
    n += 0.65 * sin(p.x * 2.3 + p.y * 1.7 + p.z * 2.1);
    n += 0.25 * sin(-p.x * 4.4 + p.y * 3.2 + p.z * 3.7);
    n += 0.10 * sin(p.x * 7.1 - p.y * 5.4 + p.z * 6.2);
    return 0.5 + 0.5 * n;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);

    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 4.8);
    float rim = smoothstep(0.64, 0.995, fresnel);
    if (rim <= 0.0005)
        discard;

    float sunFacingRaw = dot(norm, lightDir);
    float sunFacing = smoothstep(-0.18, 0.85, sunFacingRaw);
    float shellNoise = atmosphere_noise(norm * 3.2 + vec3(time * 0.05, -time * 0.04, time * 0.03));
    float haze = mix(0.82, 1.12, shellNoise);
    float forwardScatter = pow(max(dot(viewDir, lightDir), 0.0), 7.0);
    float strongForwardScatter = pow(max(dot(viewDir, lightDir), 0.0), 20.0);
    float horizonBand = pow(clamp(1.0 - abs(sunFacingRaw), 0.0, 1.0), 2.2);
    float nightRim = rim * (1.0 - sunFacing) * (0.28 + 0.40 * shellNoise);
    float dayRim = rim * sunFacing * haze;

    float dayGlow = dayRim * (0.10 + 0.72 * sunFacing);
    float twilight = rim * smoothstep(-0.48, 0.14, sunFacingRaw) * horizonBand * 0.24;
    float sunBloom = rim * (0.18 + 1.30 * forwardScatter + 1.90 * strongForwardScatter) * (0.32 + 0.68 * sunFacing);
    float silverLining = rim * smoothstep(0.60, 0.97, sunFacing) * (0.16 + 0.88 * strongForwardScatter);

    vec3 coolColor = lightColor * vec3(0.18, 0.34, 0.78);
    vec3 warmColor = lightColor * vec3(1.0, 0.66, 0.28);
    vec3 twilightColor = lightColor * vec3(1.0, 0.48, 0.22);
    vec3 silverColor = lightColor * vec3(1.18, 1.02, 0.92);
    vec3 color = coolColor * dayGlow * 0.30;
    color += coolColor * nightRim * 0.07;
    color += warmColor * sunBloom * haze * 0.88;
    color += twilightColor * twilight * (0.34 + 0.18 * shellNoise);
    color += silverColor * silverLining * 0.22;

    if (dot(color, color) < 0.0002)
        discard;

    FragColor = vec4(color * intensity * 0.24, 1.0);
}
