#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D inputTexture;
uniform vec2 blurDirection;
uniform int blurMode;
uniform float blurRadiusScale;

float sample_scalar(vec2 uv)
{
    vec4 s = texture(inputTexture, uv);
    return blurMode == 0 ? max(max(s.r, s.g), s.b) : s.r;
}

float sample_validity(vec2 uv)
{
    if (blurMode == 0)
        return 1.0;

    float value = sample_scalar(uv);
    return (value > 0.000001 && value < 0.999999) ? 1.0 : 0.0;
}

void main()
{
    if (length(blurDirection) <= 0.000001) {
        float value = sample_scalar(TexCoord);
        FragColor = vec4(value, value, value, 1.0);
        return;
    }

    vec2 texel = 1.0 / vec2(textureSize(inputTexture, 0));
    vec2 offset = blurDirection * texel * max(blurRadiusScale, 1.0);

    if (blurMode == 1) {
        float value = 0.0;
        float weight = 0.0;
        float sampleValue = sample_scalar(TexCoord);
        float sampleWeight = 0.163967 * sample_validity(TexCoord);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;

        sampleValue = sample_scalar(TexCoord + offset * 1.458430);
        sampleWeight = 0.233308 * sample_validity(TexCoord + offset * 1.458430);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;
        sampleValue = sample_scalar(TexCoord - offset * 1.458430);
        sampleWeight = 0.233308 * sample_validity(TexCoord - offset * 1.458430);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;

        sampleValue = sample_scalar(TexCoord + offset * 3.403985);
        sampleWeight = 0.096945 * sample_validity(TexCoord + offset * 3.403985);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;
        sampleValue = sample_scalar(TexCoord - offset * 3.403985);
        sampleWeight = 0.096945 * sample_validity(TexCoord - offset * 3.403985);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;

        sampleValue = sample_scalar(TexCoord + offset * 5.351806);
        sampleWeight = 0.018138 * sample_validity(TexCoord + offset * 5.351806);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;
        sampleValue = sample_scalar(TexCoord - offset * 5.351806);
        sampleWeight = 0.018138 * sample_validity(TexCoord - offset * 5.351806);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;

        sampleValue = sample_scalar(TexCoord + offset * 7.302940);
        sampleWeight = 0.001626 * sample_validity(TexCoord + offset * 7.302940);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;
        sampleValue = sample_scalar(TexCoord - offset * 7.302940);
        sampleWeight = 0.001626 * sample_validity(TexCoord - offset * 7.302940);
        value += sampleValue * sampleWeight;
        weight += sampleWeight;

        value = weight > 0.000001 ? value / weight : 1.0;
        FragColor = vec4(value, value, value, 1.0);
        return;
    }

    float value = sample_scalar(TexCoord) * 0.163967;
    value += sample_scalar(TexCoord + offset * 1.458430) * 0.233308;
    value += sample_scalar(TexCoord - offset * 1.458430) * 0.233308;
    value += sample_scalar(TexCoord + offset * 3.403985) * 0.096945;
    value += sample_scalar(TexCoord - offset * 3.403985) * 0.096945;
    value += sample_scalar(TexCoord + offset * 5.351806) * 0.018138;
    value += sample_scalar(TexCoord - offset * 5.351806) * 0.018138;
    value += sample_scalar(TexCoord + offset * 7.302940) * 0.001626;
    value += sample_scalar(TexCoord - offset * 7.302940) * 0.001626;

    FragColor = vec4(value, value, value, 1.0);
}
