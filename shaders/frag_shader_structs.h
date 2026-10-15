#ifndef FRAG_SHADER_STRUCTS_H
#define FRAG_SHADER_STRUCTS_H

struct PointLight {
    vec4 position;
    vec4 color;
    float linear;
    float quadratic;
    vec2 padding;
};

struct SpotLight {
    vec4 position;
    vec4 direction;
    vec4 color;
    float innerCutoff;
    float outerCutoff;
    float linear;
    float quadratic;
};

#endif // FRAG_SHADER_STRUCTS_H