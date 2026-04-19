#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;

float ocean_noise(vec3 p)
{
    float n = 0.0;
    n += 0.6 * sin(p.x * 3.2 + p.z * 2.5 + time * 0.18);
    n += 0.3 * sin(-p.x * 6.1 + p.y * 5.3 + p.z * 4.1 - time * 0.11);
    n += 0.1 * sin(p.x * 11.6 - p.z * 8.3 + p.y * 7.8 + time * 0.07);
    return 0.5 + 0.5 * n;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);

    float waveA = ocean_noise(norm * 8.0 + vec3(time * 0.08, 0.0, -time * 0.06));
    float waveB = ocean_noise(norm.zxy * 11.0 + vec3(-time * 0.05, time * 0.07, 0.0));
    float waves = smoothstep(0.18, 0.92, mix(waveA, waveB, 0.45));

    float diffuse = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflect(-lightDir, norm)), 0.0), 56.0);
    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.6);

    vec3 deepColor = lightColor * vec3(0.02, 0.12, 0.34);
    vec3 shallowColor = lightColor * vec3(0.08, 0.35, 0.78);
    vec3 foamColor = lightColor * vec3(0.75, 0.92, 1.1);

    vec3 color = mix(deepColor, shallowColor, waves) * (0.08 + diffuse * 0.3);
    color += foamColor * spec * 0.65;
    color += shallowColor * fresnel * (0.12 + 0.18 * waves);

    if (dot(color, color) < 0.0003)
        discard;

    FragColor = vec4(color * intensity * 0.55, 1.0);
}
