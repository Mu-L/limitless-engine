#define LIGHT_TYPE_POINT 0u
#define LIGHT_TYPE_SPOT 1u

struct Light {
    // rgb - linear color, a - intensity
    vec4 color;
    // world position
    vec3 position;
    // maximum radius
    float radius;
    // spot light attenuation
    vec2 scale_offset;
    // light type
    uint type;
    // does light cast shadow
    bool casts_shadow;
    // falloff
    float falloff;
    // spot direction
    vec3 direction;
};

/*
    if opengl's SSBO extension is supported, all lights stored in SSBO

    if there is no SSBO, then engine defines MAX_LIGHTS
    which is guaranteed to be less or equal then UNIFORM MAX SIZE

    so actually there is limit on lights which is bound by uniform/ssbo max size divided by sizeof(light)
*/

#if defined (ENGINE_EXT_SSBO)
    layout (std140) buffer LIGHTS_BUFFER {
        Light _lights[];
    };
#else
    //TODO: remove to settigns
    #define ENGINE_SETTINGS_MAX_LIGHTS 100

    layout (std140) uniform LIGHTS_BUFFER {
        Light _lights[ENGINE_SETTINGS_MAX_LIGHTS];
    };
#endif

Light getLight(const uint index) {
    return _lights[index];
}
