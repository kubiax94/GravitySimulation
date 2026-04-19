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
    n += 0.6 * sin(p.x * 4.2 + p.y * 2.3 + p.z * 3.1);
    n += 0.3 * sin(-p.x * 7.6 + p.y * 5.1 + p.z * 4.4);
    n += 0.1 * sin(p.x * 12.4 - p.y * 8.0 + p.z * 9.5);
    return 0.5 + 0.5 * n;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);

    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 2.7);
    float rim = smoothstep(0.08, 0.98, fresnel);
    if (rim <= 0.001)
        discard;

    float sunFacing = max(dot(norm, lightDir), 0.0);
    float shellNoise = atmosphere_noise(norm * 8.0 + vec3(time * 0.18, -time * 0.12, time * 0.08));
    float breakup = smoothstep(0.25, 0.92, shellNoise);

    float dayGlow = rim * (0.35 + 0.95 * pow(sunFacing, 1.25));
    float twilight = rim * (1.0 - sunFacing) * 0.65;

    vec3 coolColor = lightColor * vec3(0.32, 0.6, 1.25);
    vec3 warmColor = lightColor * vec3(1.15, 0.65, 0.28);
    vec3 color = coolColor * dayGlow * breakup;
    color += warmColor * twilight * (0.45 + 0.55 * shellNoise);

    if (dot(color, color) < 0.0002)
        discard;

    FragColor = vec4(color * intensity * 0.9, 1.0);
}
