// shader.frag

#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

struct PointLight {
    vec4 position;
    vec4 color;
};

struct SpotLight {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec2 angles;
    vec2 padding;
};

layout(std140, set = 0, binding = 0) uniform SceneData {
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

layout(std430, set = 0, binding = 1) readonly buffer PointLightBuffer {
    PointLight pointLights[];
};

layout(std430, set = 0, binding = 2) readonly buffer SpotLightBuffer {
    SpotLight spotLights[];
};

layout(std140, set = 1, binding = 0) uniform ObjectData {
    mat4 model;
    vec4 albedo;
    vec4 specularColor;
    float shininess;
} material;


void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.cameraPos.xyz - fragPos);

    vec3 finalColor = scene.ambientLight.rgb * material.albedo.rgb;

    vec3 L_dir = normalize(-scene.dirLightDir.xyz);
    float diff_dir = max(dot(N, L_dir), 0.0);
    vec3 diffuse_dir = diff_dir * material.albedo.rgb * scene.dirLightColor.rgb;
    
    vec3 H_dir = normalize(L_dir + V);
    float spec_dir = pow(max(dot(N, H_dir), 0.0), material.shininess);
    vec3 specular_dir = spec_dir * material.specularColor.rgb * scene.dirLightColor.rgb;
    
    finalColor += (diffuse_dir + specular_dir) * scene.dirLightColor.a;


    for (uint i = 0; i < scene.pointLightCount; ++i) {
        PointLight light = pointLights[i];

        vec3 L = light.position.xyz - fragPos;
        float dist = length(L);
        L = normalize(L);

        float attenuation = 1.0 / (dist * dist); 

        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * material.albedo.rgb * light.color.rgb;

        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), material.shininess);
        vec3 specular = spec * material.specularColor.rgb * light.color.rgb;

        finalColor += (diffuse + specular) * attenuation * light.color.a;
    }

    // TODO: spotLight

    outColor = vec4(finalColor, 1.0);
}