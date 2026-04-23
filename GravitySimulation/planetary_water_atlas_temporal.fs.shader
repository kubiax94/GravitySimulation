#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D currentAtlasTexture;
uniform sampler2D historyAtlasTexture;
uniform float historyBlend;

vec4 sample_atlas(sampler2D atlasTexture, vec2 uv) {
    vec2 wrapped = vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0));
    return texture(atlasTexture, wrapped);
}

void main() {
    vec4 currentAtlas = sample_atlas(currentAtlasTexture, TexCoord);
    vec4 historyAtlas = sample_atlas(historyAtlasTexture, TexCoord);

    float currentWeight = max(currentAtlas.r, 0.0);
    float historyWeight = max(historyAtlas.r, 0.0);
    float currentPresence = smoothstep(0.0015, 0.02, currentWeight);
    float historyPresence = smoothstep(0.003, 0.03, historyWeight);
    float historyRetain = mix(0.996, 0.9992, clamp(historyBlend, 0.0, 1.0));
    vec4 retainedHistory = historyAtlas * historyRetain;
    vec4 blendedAtlas = max(currentAtlas, retainedHistory);

    if (currentPresence < historyPresence)
        blendedAtlas = max(blendedAtlas, historyAtlas * mix(0.998, 0.9996, historyPresence));

    blendedAtlas.r = max(blendedAtlas.r, max(currentWeight, historyWeight * historyRetain));

    FragColor = blendedAtlas;
}
