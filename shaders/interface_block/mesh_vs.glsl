out _vertex_data {
    #if defined (ENGINE_MATERIAL_NORMAL_TEXTURE) && defined (ENGINE_SETTINGS_NORMAL_MAPPING)
        mat3 TBN;
    #else
        vec3 normal;
    #endif

    vec3 world_position;
    vec2 uv;

    #if defined (ENGINE_MATERIAL_INSTANCED_MODEL)
        flat int instance_id;
    #endif
} _out_data;
