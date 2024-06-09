ENGINE::COMMON

#include "../functions/tone_mapping.glsl"

in vec2 uv;

out vec3 color;

uniform sampler2D lightened;

uniform sampler2D bloom;
uniform float bloom_strength;
uniform float tone_mapping_exposure = 1.0;

void main() {
    vec3 bloom_color = texture(bloom, uv).rgb * bloom_strength;
    color = texture(lightened, uv).rgb + bloom_color;

    color = toneMapping(color, tone_mapping_exposure);
}