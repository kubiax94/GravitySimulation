#version 460 core

out vec4 FragColor;

uniform vec3 particleColor;

void main() {
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radiusSq = dot(centered, centered);
    if (radiusSq > 1.0)
        discard;

    float glow = 1.0 - radiusSq;
    FragColor = vec4(particleColor * (0.6 + 0.4 * glow), 1.0);
}
