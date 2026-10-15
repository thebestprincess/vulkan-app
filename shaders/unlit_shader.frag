//unlit_shader.frag

#version 450

#include "common.h"

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(object.albedo.rgb, 1.0); 
}