#version 450

#include "common.h"

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform ShadowPushConstants {
    mat4 lightViewProj;
} pc;

void main() {
    gl_Position = pc.lightViewProj * object.model * vec4(inPosition, 1.0);
}