#version 460 core

in float AtlasWeight;
in float AtlasDepth;
in float AtlasCarrier;
in float AtlasFlood;

out vec4 FragColor;

void main() {
    if (AtlasWeight <= 0.0001)
        discard;

    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radiusSq = dot(centered, centered);
    if (radiusSq > 1.0)
        discard;

    float falloff = pow(max(1.0 - radiusSq, 0.0), 2.35);
    float weight = AtlasWeight * falloff;
    if (weight <= 0.0001)
        discard;

    FragColor = vec4(
        weight,
        AtlasDepth * weight,
        AtlasCarrier * weight,
        AtlasFlood * weight);
}
