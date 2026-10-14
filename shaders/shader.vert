// shader.vert

#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;

layout(set = 0, binding = 0) uniform SceneUniform {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 ambientLight;
    vec4 dirLightDir;
    vec4 dirLightColor;
    uint pointLightCount;
    uint spotLightCount;
    vec2 padding;
} scene;

layout(set = 1, binding = 0) uniform ObjectUniform {
    mat4 model;
    vec4 albedo;
    vec4 specularColor;
    float shininess;
} object;

void main() {
    vec4 worldPosition = object.model * vec4(inPosition, 1.0);
    fragPos = worldPosition.xyz;

    mat3 normalMatrix = mat3(transpose(inverse(object.model)));
    fragNormal = normalMatrix * inNormal;

    gl_Position = scene.proj * scene.view * worldPosition;
}