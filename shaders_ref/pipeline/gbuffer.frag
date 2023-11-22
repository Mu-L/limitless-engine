ENGINE::COMMON
ENGINE::MATERIALDEPENDENT

#include "../interface_block/fragment.glsl"
#include "../scene.glsl"
#include "../material/material_context.glsl"

layout (location = 0) out vec4 albedo;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec3 properties;
layout (location = 3) out vec3 emissive;

void main() {
    const MaterialContext mctx = computeMaterialContext();

    albedo.rgb = computeMaterialColor(mctx).rgb;
    albedo.a = computeMaterialAO(mctx);

    normal = computeMaterialNormal(mctx);

    properties.r = mctx.roughness;
    properties.g = mctx.metallic;
    properties.b = float(mctx.shading_model) / 255.0;

    emissive = computeMaterialEmissiveColor(mctx);
}
