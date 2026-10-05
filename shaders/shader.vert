// shader.vert

#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform SceneUniform {
    mat4 view;
    mat4 proj;
} scene;

layout(set = 1, binding = 0) uniform ObjectUniform {
    mat4 model;
    vec4 color;
} object;

void main() {
    gl_Position = scene.proj * scene.view * object.model * vec4(inPosition, 1.0);
    
    vec3 localColor = inPosition + vec3(0.5); 
    
    fragColor = localColor * object.color.rgb;
}