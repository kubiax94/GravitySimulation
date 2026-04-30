#version 460 core

in float AtlasWeight;
in float AtlasDepth;
in float AtlasCarrier;
in float AtlasFlood;
in float AtlasWaveForcing;
in float AtlasWaveCarrier;

out vec4 FragColor;
layout(location = 1) out vec4 WaveForcingColor;

void main() {
    if (AtlasWeight <= 0.0001)
        discard;

    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radiusSq = dot(centered, centered);
    if (radiusSq > 1.0)
        discard;

    float radialDistance = sqrt(radiusSq);
    float outerFalloff = 1.0 - smoothstep(0.58, 1.0, radialDistance);
    float innerPlateau = 1.0 - smoothstep(0.0, 0.52, radialDistance);
    float falloff = clamp(outerFalloff * 0.74 + innerPlateau * 0.34, 0.0, 1.0);
    float weight = AtlasWeight * falloff;
    if (weight <= 0.0001)
        discard;

    FragColor = vec4(
        weight,
        AtlasDepth * weight,
        AtlasCarrier * weight,
        AtlasFlood * weight);
    WaveForcingColor = vec4(
        AtlasWaveForcing * weight,
        AtlasWaveCarrier * weight,
        0.0,
        weight);
}
