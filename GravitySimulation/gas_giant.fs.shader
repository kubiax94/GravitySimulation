#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;

float wave_noise(vec3 p)
{
    float n = 0.0;
    n += sin(p.x * 2.4 + p.y * 5.8 + p.z * 2.9);
    n += 0.5 * sin(-p.x * 4.6 + p.y * 9.4 + p.z * 6.1);
    n += 0.25 * sin(p.x * 11.2 - p.y * 15.0 + p.z * 7.3);
    return n / 1.75;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);

    float latitude = norm.y;
    float azimuth = atan(norm.z, norm.x);
    float bands = 0.5 + 0.5 * sin(latitude * 22.0 + wave_noise(vec3(azimuth * 2.2, latitude * 5.0, time * 0.22)) * 3.0 - time * 0.35);
    float storms = 0.5 + 0.5 * wave_noise(vec3(norm.x * 7.5 + time * 0.08, latitude * 12.5 - time * 0.12, norm.z * 6.0));
    float bandMask = smoothstep(0.14, 0.88, bands);

    vec3 deepColor = lightColor * vec3(0.65, 0.48, 0.22);
    vec3 midColor = lightColor * vec3(1.05, 0.78, 0.42);
    vec3 brightColor = lightColor * vec3(1.28, 0.98, 0.72);
    vec3 baseColor = mix(deepColor, midColor, bandMask);
    baseColor = mix(baseColor, brightColor, storms * 0.4);

    float ambient = 0.08;
    float diffuse = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflect(-lightDir, norm)), 0.0), 20.0);
    float rim = pow(1.0 - max(dot(norm, viewDir), 0.0), 2.2);

    vec3 color = baseColor * (ambient + diffuse * 0.95);
    color += brightColor * spec * 0.22;
    color += brightColor * rim * 0.08;

    FragColor = vec4(color * intensity, 1.0);
}
