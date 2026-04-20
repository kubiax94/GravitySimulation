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
    n += sin(p.x * 2.7 + p.y * 3.4 + p.z * 2.1);
    n += 0.5 * sin(-p.x * 5.8 + p.y * 4.9 + p.z * 6.2);
    n += 0.25 * sin(p.x * 10.7 - p.y * 9.1 + p.z * 7.5);
    return n / 1.75;
}

float fbm(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.55;
    float frequency = 1.0;

    for (int i = 0; i < 5; ++i) {
        value += amplitude * wave_noise(p * frequency);
        frequency *= 1.95;
        amplitude *= 0.5;
        p = p.yzx + vec3(0.37, -0.21, 0.43);
    }

    return value;
}

float crater_mask(vec3 p)
{
    float a = 0.5 + 0.5 * sin(p.x * 18.0 + p.y * 11.0 + p.z * 14.0);
    float b = 0.5 + 0.5 * sin(-p.x * 23.0 + p.y * 19.0 - p.z * 17.0);
    float c = 0.5 + 0.5 * sin(p.x * 29.0 - p.y * 27.0 + p.z * 21.0);
    return smoothstep(0.78, 0.97, a * b * c);
}

float terrain_height(vec3 n)
{
    float largeScale = 0.5 + 0.5 * fbm(n * 4.2);
    float mediumScale = 0.5 + 0.5 * fbm(n.zxy * 9.5 + vec3(1.7, -2.1, 0.9));
    float detailScale = 0.5 + 0.5 * fbm(n.yzx * 18.0 + vec3(-3.2, 1.4, 2.6));
    float ridgeMask = pow(1.0 - abs(fbm(n * 13.0 + vec3(0.4, -0.8, 1.1))), 2.3);
    float craters = crater_mask(n * 1.3 + vec3(0.4, -0.6, 1.2));

    float height = largeScale * 0.55
        + mediumScale * 0.27
        + detailScale * 0.12
        + ridgeMask * 0.06;

    return height - craters * 0.18;
}

vec3 perturb_terrain_normal(vec3 n)
{
    vec3 helperAxis = abs(n.y) > 0.82 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 tangent = normalize(cross(helperAxis, n));
    vec3 bitangent = normalize(cross(n, tangent));

    const float offset = 0.03;
    const float strength = 2.1;

    float baseHeight = terrain_height(n);
    float tangentHeight = terrain_height(normalize(n + tangent * offset));
    float bitangentHeight = terrain_height(normalize(n + bitangent * offset));

    vec3 gradient = (tangentHeight - baseHeight) * tangent + (bitangentHeight - baseHeight) * bitangent;
    return normalize(n - gradient * strength);
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 terrainNorm = perturb_terrain_normal(norm);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(lightPos - FragPos);

    float terrain = terrain_height(norm);
    float terrainA = 0.5 + 0.5 * fbm(norm * 4.2);
    float terrainB = 0.5 + 0.5 * fbm(norm.zxy * 9.5 + vec3(1.7, -2.1, 0.9));
    float continents = smoothstep(0.36, 0.62, terrain);
    float mountains = smoothstep(0.56, 0.82, terrain);
    float ridges = pow(1.0 - abs(fbm(norm * 13.0 + vec3(0.4, -0.8, 1.1))), 2.4);
    float craters = crater_mask(norm * 1.3 + vec3(0.4, -0.6, 1.2));
    float iceCaps = smoothstep(0.62, 0.9, abs(norm.y));

    vec3 rockDark = lightColor * vec3(0.12, 0.08, 0.05);
    vec3 rockMid = lightColor * vec3(0.38, 0.24, 0.13);
    vec3 rockBright = lightColor * vec3(0.73, 0.55, 0.33);
    vec3 dustColor = lightColor * vec3(0.61, 0.33, 0.15);
    vec3 iceColor = lightColor * vec3(0.76, 0.84, 0.92);

    vec3 baseColor = mix(rockDark, rockMid, continents);
    baseColor = mix(baseColor, dustColor, (1.0 - continents) * (0.25 + 0.75 * terrainB));
    baseColor = mix(baseColor, rockBright, mountains * 0.75 + ridges * 0.18);
    baseColor = mix(baseColor, rockDark * 0.45, craters * 0.82);
    baseColor = mix(baseColor, iceColor, iceCaps * (0.25 + 0.75 * mountains));

    float ambient = 0.08;
    float diffuse = max(dot(terrainNorm, lightDir), 0.0);
    float specular = pow(max(dot(viewDir, reflect(-lightDir, terrainNorm)), 0.0), 28.0);
    float rim = pow(1.0 - max(dot(terrainNorm, viewDir), 0.0), 2.2);

    vec3 color = baseColor * (ambient + diffuse * 1.12);
    color += rockBright * specular * (0.05 + 0.14 * mountains + 0.05 * ridges);
    color += dustColor * rim * 0.05;

    FragColor = vec4(color * intensity, 1.0);
}
