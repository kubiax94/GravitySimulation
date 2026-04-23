#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;
uniform float time;
uniform float cloudCoverage;
uniform float cloudSoftness;
uniform float cloudOpacity;
uniform float cloudSpeed;
uniform vec3 cloudColor;
uniform vec3 cloudShadowColor;

float cloud_noise(vec3 p)
{
    float n = 0.0;
    n += 0.6 * sin(p.x * 2.1 + p.y * 1.8 + p.z * 2.4);
    n += 0.3 * sin(-p.x * 4.3 + p.y * 3.5 + p.z * 3.8);
    n += 0.1 * sin(p.x * 7.2 - p.y * 5.1 + p.z * 6.4);
    return n / 1.75;
}

float fbm(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.55;
    float frequency = 1.0;

    for (int i = 0; i < 5; ++i) {
        value += amplitude * cloud_noise(p * frequency);
        frequency *= 1.9;
        amplitude *= 0.5;
        p = p.yzx + vec3(0.31, -0.24, 0.17);
    }

    return value;
}

float cloud_field(vec3 n)
{
    vec3 warped = normalize(n + vec3(
        fbm(n * 2.1 + vec3(time * cloudSpeed * 0.35, 0.0, -time * cloudSpeed * 0.22)),
        fbm(n.zxy * 2.7 + vec3(-0.6, time * cloudSpeed * 0.28, 0.3)),
        fbm(n.yzx * 3.1 + vec3(0.7, -0.4, time * cloudSpeed * 0.18))) * 0.18);

    float primary = 0.5 + 0.5 * fbm(warped * 3.4 + vec3(time * cloudSpeed, 0.0, -time * cloudSpeed * 0.8));
    float secondary = 0.5 + 0.5 * fbm(warped.zxy * 5.2 + vec3(-time * cloudSpeed * 0.55, time * cloudSpeed * 0.35, 0.0));
    float breakup = 0.5 + 0.5 * fbm(warped.yzx * 8.0 + vec3(0.9, -1.2, 0.4));
    return primary * 0.62 + secondary * 0.28 + breakup * 0.10;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);

    float cloudField = cloud_field(norm);
    float cloudMask = smoothstep(cloudCoverage - cloudSoftness, cloudCoverage + cloudSoftness, cloudField);
    if (cloudMask <= 0.001)
        discard;

    float diffuse = max(dot(norm, lightDir), 0.0);
    float rim = pow(1.0 - max(dot(norm, viewDir), 0.0), 2.6);
    float forwardScatter = pow(max(dot(viewDir, lightDir), 0.0), 6.0);

    vec3 color = mix(cloudShadowColor, cloudColor, 0.25 + 0.75 * diffuse);
    color += cloudColor * forwardScatter * 0.45;
    color += cloudColor * rim * 0.12;

    float alpha = cloudMask * cloudOpacity * (0.35 + 0.65 * diffuse + 0.2 * forwardScatter);
    FragColor = vec4(color * lightColor * intensity, alpha);
}
