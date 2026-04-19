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
    n += sin(p.x * 3.5 + p.z * 2.1 + time * 0.25);
    n += 0.5 * sin(-p.x * 6.4 + p.y * 5.2 + p.z * 4.3 - time * 0.18);
    n += 0.25 * sin(p.x * 12.0 - p.z * 8.6 + p.y * 9.1 + time * 0.11);
    return n / 1.75;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);

    float cloudMask = 0.5 + 0.5 * ocean_noise(norm * 7.5 + vec3(time * 0.05, 0.0, -time * 0.04));
    cloudMask = smoothstep(0.58, 0.9, cloudMask);

    vec3 deepOcean = lightColor * vec3(0.08, 0.25, 0.82);
    vec3 shallowOcean = lightColor * vec3(0.12, 0.58, 1.08);
    vec3 cloudColor = lightColor * vec3(1.08, 1.1, 1.14);

    float ambient = 0.08;
    float diffuse = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflect(-lightDir, norm)), 0.0), 42.0);
    float fresnel = pow(1.0 - max(dot(norm, viewDir), 0.0), 3.0);

    vec3 oceanColor = mix(deepOcean, shallowOcean, 0.5 + 0.5 * norm.y);
    vec3 color = oceanColor * (ambient + diffuse);
    color += cloudColor * cloudMask * (0.2 + diffuse * 0.55);
    color += lightColor * spec * 0.55;
    color += shallowOcean * fresnel * 0.18;

    FragColor = vec4(color * intensity, 1.0);
}
