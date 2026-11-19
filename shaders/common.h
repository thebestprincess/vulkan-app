#ifndef SHADERS_COMMON_H
#define SHADERS_COMMON_H

#define MAX_SPOT_SHADOWS 8

layout(set = 0, binding = 0) uniform SceneUniform {
    mat4 view;
    mat4 proj;
    mat4 dirLightSpaceMatrix;
    mat4 spotLightSpaceMatrices[MAX_SPOT_SHADOWS];
    vec4 cameraPos;
    vec4 ambientLight;
    vec4 dirLightDir;
    vec4 dirLightColor;
    uint pointLightCount;
    uint spotLightCount;
    uint spotShadowCount;
    uint padding;
} scene;

layout(set = 1, binding = 0) uniform ObjectUniform {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedo;
    vec4 specularColor;
    float shininess;
} object;

#endif // SHADERS_COMMON_H