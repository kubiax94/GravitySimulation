#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D inputTexture;
uniform vec2 blurDirection;
uniform float blurRadiusScale;

vec4 sample_atlas(vec2 uv) {
    vec2 wrapped = vec2(fract(uv.x + 1.0), clamp(uv.y, 0.0, 1.0));
    return texture(inputTexture, wrapped);
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(inputTexture, 0));
    vec2 offset = blurDirection * texel * max(blurRadiusScale, 1.0);

    vec4 value = sample_atlas(TexCoord) * 0.227027;
    value += sample_atlas(TexCoord + offset * 1.384615) * 0.316216;
    value += sample_atlas(TexCoord - offset * 1.384615) * 0.316216;
    value += sample_atlas(TexCoord + offset * 3.230769) * 0.070270;
    value += sample_atlas(TexCoord - offset * 3.230769) * 0.070270;

    FragColor = value;
}
