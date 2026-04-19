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

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    float ndotV = max(dot(norm, viewDir), 0.0);
    float rim = pow(1.0 - ndotV, 3.0);
    rim = smoothstep(0.24, 0.98, rim);
    if (rim <= 0.001)
        discard;

    float azimuth = atan(norm.z, norm.x);
    float latitude = norm.y;
    float flowA = max(sin(azimuth * 4.5 + time * 0.9 + latitude * 7.0), 0.0);
    float flowB = max(sin(azimuth * 7.5 - time * 1.25 - latitude * 9.0), 0.0);
    float loopMask = pow(max(flowA, flowB), 8.0);

    float shellNoise = halo_noise(norm * 8.5 + vec3(time * 1.15, -time * 0.7, time * 0.35));
    float breakup = smoothstep(0.42, 0.92, shellNoise);
    float tongues = rim * rim * loopMask * breakup;

    float outerCorona = rim * (0.25 + 0.75 * shellNoise);
    vec3 flareColor = lightColor * vec3(1.55, 0.82, 0.28);
    vec3 hotColor = lightColor * vec3(2.0, 1.18, 0.42);

    vec3 color = flareColor * outerCorona * (0.45 + 0.2 * sin(time * 1.2 + azimuth * 3.5));
    color += mix(flareColor, hotColor, 0.65) * tongues * 1.7;

    if (dot(color, color) < 0.0005)
        discard;

    FragColor = vec4(color * intensity, 1.0);
}
