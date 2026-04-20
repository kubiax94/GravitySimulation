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

    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 4.6);
    float rim = smoothstep(0.58, 0.995, fresnel);
    if (rim <= 0.0005)
        discard;

    float sunFacingRaw = dot(norm, lightDir);
    float sunFacing = smoothstep(-0.18, 0.85, sunFacingRaw);
    float shellNoise = atmosphere_noise(norm * 3.2 + vec3(time * 0.05, -time * 0.04, time * 0.03));
    float haze = mix(0.82, 1.12, shellNoise);
    float forwardScatter = pow(max(dot(viewDir, lightDir), 0.0), 7.0);

    float dayGlow = rim * (0.08 + 0.92 * sunFacing) * haze;
    float twilight = rim * smoothstep(-0.55, 0.1, sunFacingRaw) * 0.08;
    float sunBloom = rim * (0.18 + 1.45 * forwardScatter) * (0.35 + 0.65 * sunFacing);

    vec3 coolColor = lightColor * vec3(0.18, 0.36, 0.82);
    vec3 warmColor = lightColor * vec3(1.0, 0.62, 0.24);
    vec3 color = coolColor * dayGlow * 0.35;
    color += warmColor * sunBloom * haze * 0.9;
    color += warmColor * twilight * (0.22 + 0.18 * shellNoise);

    if (dot(color, color) < 0.0002)
        discard;

    FragColor = vec4(color * intensity * 0.32, 1.0);
}
