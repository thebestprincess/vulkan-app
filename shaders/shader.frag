// shader.frag

#version 450

#include "common.h"
#include "frag_shader_structs.h"

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(std430, set = 0, binding = 1) readonly buffer PointLightBuffer {
    PointLight pointLights[];
};

layout(std430, set = 0, binding = 2) readonly buffer SpotLightBuffer {
    SpotLight spotLights[];
};

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.cameraPos.xyz - fragPos);

    vec3 finalColor = scene.ambientLight.rgb * object.albedo.rgb;

    vec3 L_dir = normalize(-scene.dirLightDir.xyz);
    float diff_dir = max(dot(N, L_dir), 0.0);
    vec3 diffuse_dir = diff_dir * object.albedo.rgb * scene.dirLightColor.rgb;
    
    vec3 H_dir = normalize(L_dir + V);
    float spec_dir = pow(max(dot(N, H_dir), 0.0), object.shininess);
    vec3 specular_dir = spec_dir * object.specularColor.rgb * scene.dirLightColor.rgb;
    
    finalColor += (diffuse_dir + specular_dir) * scene.dirLightColor.a;


    for (uint i = 0; i < scene.pointLightCount; ++i) {
        PointLight light = pointLights[i];

        vec3 L = light.position.xyz - fragPos;
        float dist = length(L);
        L = normalize(L);

        float attenuation = 1.0 / (1.0 +
                                   light.linear * dist +
                                   light.quadratic * dist * dist); 

        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * object.albedo.rgb * light.color.rgb;

        vec3 H = normalize(L + V);
        float specFactor = pow(max(dot(N, H), 0.0), object.shininess);
        if (dot(N, L) <= 0.0) specFactor = 0.0; 
        vec3 specular = specFactor * object.specularColor.rgb * light.color.rgb;

        finalColor += (diffuse + specular) * attenuation * light.color.a;
    }


    for (uint i = 0; i < scene.spotLightCount; ++i) {
        SpotLight light = spotLights[i];

        vec3 L = light.position.xyz - fragPos;
        float dist = length(L);
        L = normalize(L);

        float distAttenuation = 1.0 / (1.0 + light.linear * dist + light.quadratic * dist * dist); 

        float theta = dot(L, normalize(-light.direction.xyz));
        float epsilon = light.innerCutoff - light.outerCutoff;
        float angularAttenuation = clamp((theta - light.outerCutoff) / epsilon, 0.0, 1.0);
        if (angularAttenuation < 1e-9) { continue; }

        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * object.albedo.rgb * light.color.rgb * angularAttenuation;

        vec3 H = normalize(L + V);
        float specFactor = pow(max(dot(N, H), 0.0), object.shininess);
        if (dot(N, L) <= 0.0) specFactor = 0.0; 
        vec3 specular = specFactor * object.specularColor.rgb * light.color.rgb * angularAttenuation;

        finalColor += (diffuse + specular) * distAttenuation * light.color.a;
    }

    outColor = vec4(finalColor, 1.0);
}