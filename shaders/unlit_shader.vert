//unlit_shader.vert

#version 450

#include "common.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

void main() {
    gl_Position = scene.proj * scene.view * object.model * vec4(inPosition, 1.0);
}