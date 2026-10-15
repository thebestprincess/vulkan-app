// lit_shader.vert

#version 450

#include "common.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;

void main() {
    vec4 worldPosition = object.model * vec4(inPosition, 1.0);
    fragPos = worldPosition.xyz;

    fragNormal = mat3(object.normalMatrix) * inNormal;

    gl_Position = scene.proj * scene.view * worldPosition;
}