#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D waveStateTexture;
uniform usampler2D regionIdTexture;
uniform sampler2D shoreDistanceTexture;
uniform int waveStateTextureAvailable;
uniform int regionIdTextureAvailable;
uniform int shoreDistanceTextureAvailable;

void main()
{
    vec2 uv = vec2(fract(TexCoord.x + 1.0), clamp(TexCoord.y, 0.0, 1.0));

    vec2 waveState = waveStateTextureAvailable != 0
        ? texture(waveStateTexture, uv).rg
        : vec2(0.0);
    float amplitude = clamp(abs(waveState.x) * 8.0, 0.0, 1.0);
    float velocity = clamp(abs(waveState.y) * 2.5, 0.0, 1.0);

    vec2 texel = vec2(1.0) / vec2(max(textureSize(waveStateTexture, 0), ivec2(1)));
    vec2 waveXP = waveStateTextureAvailable != 0 ? texture(waveStateTexture, vec2(fract(uv.x + texel.x), uv.y)).rg : vec2(0.0);
    vec2 waveXM = waveStateTextureAvailable != 0 ? texture(waveStateTexture, vec2(fract(uv.x - texel.x + 1.0), uv.y)).rg : vec2(0.0);
    vec2 waveYP = waveStateTextureAvailable != 0 ? texture(waveStateTexture, vec2(uv.x, clamp(uv.y + texel.y, 0.0, 1.0))).rg : vec2(0.0);
    vec2 waveYM = waveStateTextureAvailable != 0 ? texture(waveStateTexture, vec2(uv.x, clamp(uv.y - texel.y, 0.0, 1.0))).rg : vec2(0.0);
    float gradient = clamp(length(vec2(waveXP.x - waveXM.x, waveYP.x - waveYM.x)) * 8.0, 0.0, 1.0);

    float shore = shoreDistanceTextureAvailable != 0
        ? clamp(texture(shoreDistanceTexture, uv).r * 10.0, 0.0, 1.0)
        : 0.0;

    uint region = regionIdTextureAvailable != 0
        ? texelFetch(regionIdTexture, ivec2(
            clamp(int(floor(uv.x * float(textureSize(regionIdTexture, 0).x))), 0, textureSize(regionIdTexture, 0).x - 1),
            clamp(int(floor(uv.y * float(textureSize(regionIdTexture, 0).y))), 0, textureSize(regionIdTexture, 0).y - 1)), 0).r
        : 0u;

    vec3 color = vec3(amplitude, gradient, velocity);
    color = mix(vec3(0.05, 0.0, 0.08), color, 0.92);
    color += vec3(0.0, 0.18, 0.08) * shore;

    if (region == 0u)
        color = vec3(0.25, 0.0, 0.25);

    float grid = step(0.985, fract(uv.x * 32.0)) + step(0.985, fract(uv.y * 16.0));
    color += vec3(0.2, 0.2, 0.2) * clamp(grid, 0.0, 1.0);

    FragColor = vec4(color, 0.92);
}
