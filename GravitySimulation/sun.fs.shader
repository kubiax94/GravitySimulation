#version 460 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float intensity;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-normalize(FragPos), norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    vec3 emissive = lightColor * intensity;
    vec3 specular = spec * lightColor;

    FragColor = vec4(emissive + specular, 1.0);
}