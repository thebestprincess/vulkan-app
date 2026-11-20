#version 450

#include "common.h"
#include "frag_shader_structs.h"

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(std430, set = 0, binding = 1) readonly buffer PointLightBuffer {
    PointLight pointLights[];
};

layout(std430, set = 0, binding = 2) readonly buffer SpotLightBuffer {
    SpotLight spotLights[];
};

layout(set = 0, binding = 3) uniform sampler2DShadow dirShadowMap;
layout(set = 0, binding = 4) uniform sampler2DArrayShadow spotShadowMaps;

layout(set = 2, binding = 0) uniform sampler2D albedoSampler;
layout(set = 2, binding = 1) uniform sampler2D specSampler;
layout(set = 2, binding = 2) uniform sampler2D emissiveSampler;

layout(push_constant) uniform PushConstants {
    uint isUnlit;                 // 1 - unlit, 0 - lit
    float time;                   // For non-trivial sampling
    uint enableComplexSampling;
} pc;

float calculate_shadow(sampler2DShadow shadowMap, mat4 lightSpaceMatrix, vec3 worldPos)
{
    vec4 lightSpacePos = lightSpaceMatrix * vec4(worldPos, 1.0);

    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    return texture(shadowMap, projCoords);
}

float calculate_spot_shadow(uint layer, mat4 lightSpaceMatrix, vec3 worldPos)
{
    vec4 lightSpacePos = lightSpaceMatrix * vec4(worldPos, 1.0);
    if (lightSpacePos.w <= 0.0)
    {
        return 1.0;
    }

    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    return texture(spotShadowMaps, vec4(projCoords.xy, float(layer), projCoords.z));
}

void main() {
    vec2 uvR = fragUV, uvG = fragUV, uvB = fragUV;

    if (pc.enableComplexSampling == 1)
    {
        float wave = sin(fragUV.y * 10.0 + pc.time * 3.0) * 0.015;
        uvR = fragUV + vec2(wave + 0.01, 0.0);
        uvG = fragUV + vec2(wave, 0.0);
        uvB = fragUV + vec2(wave - 0.01, 0.0);
    }

    float aR = texture(albedoSampler, uvR).r;
    float aG = texture(albedoSampler, uvG).g;
    float aB = texture(albedoSampler, uvB).b;
    vec3 albedo = vec3(aR, aG, aB) * object.albedo.rgb;     // Albedo

    if (pc.isUnlit == 1)
    {
        outColor = vec4(albedo, 1.0);
        return;
    }

    float specMask = texture(specSampler, uvG).r;           // Specular mask

    float eR = texture(emissiveSampler, uvR).r;
    float eG = texture(emissiveSampler, uvG).g;
    float eB = texture(emissiveSampler, uvB).b;
    vec3 emissiveColor = vec3(eR, eG, eB);                  // Emissive color

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.cameraPos.xyz - fragPos);

    vec3 finalColor = scene.ambientLight.rgb * albedo;

    // Direction light: Sun + shadow
    vec3 L_dir = normalize(-scene.dirLightDir.xyz);
    float diff_dir = max(dot(N, L_dir), 0.0);
    vec3 diffuse_dir = diff_dir * albedo * scene.dirLightColor.rgb;

    vec3 H_dir = normalize(L_dir + V);
    float spec_dir = pow(max(dot(N, H_dir), 0.0), object.shininess);
    if (dot(N, L_dir) <= 0.0) spec_dir = 0.0;
    vec3 specular_dir = spec_dir * object.specularColor.rgb * scene.dirLightColor.rgb * specMask;

    float dirShadow = calculate_shadow(dirShadowMap, scene.dirLightSpaceMatrix, fragPos);
    finalColor += (diffuse_dir + specular_dir) * scene.dirLightColor.a * dirShadow;

    for (uint i = 0; i < scene.pointLightCount; ++i) {
        PointLight light = pointLights[i];

        vec3 L = light.position.xyz - fragPos;
        float dist = length(L);
        L = normalize(L);

        float attenuation = 1.0 / (1.0 +
                                   light.linear * dist +
                                   light.quadratic * dist * dist);

        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * albedo * light.color.rgb;

        vec3 H = normalize(L + V);
        float specFactor = pow(max(dot(N, H), 0.0), object.shininess);
        if (dot(N, L) <= 0.0) specFactor = 0.0;
        vec3 specular = specFactor * object.specularColor.rgb * light.color.rgb * specMask;

        finalColor += (diffuse + specular) * attenuation * light.color.a;
    }

    for (uint i = 0; i < scene.spotLightCount; ++i) {
        SpotLight light = spotLights[i];

        vec3 L = light.position.xyz - fragPos;
        float dist = length(L);
        L = normalize(L);

        float distAttenuation = 1.0 / (1.0 +
                                   light.linear * dist +
                                   light.quadratic * dist * dist);

        float theta = dot(L, normalize(-light.direction.xyz));
        float epsilon = light.innerCutoff - light.outerCutoff;
        float angularAttenuation = clamp((theta - light.outerCutoff) / epsilon, 0.0, 1.0);
        if (angularAttenuation < 1e-9) { continue; }

        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * albedo * light.color.rgb * angularAttenuation;

        vec3 H = normalize(L + V);
        float specFactor = pow(max(dot(N, H), 0.0), object.shininess);
        if (dot(N, L) <= 0.0) specFactor = 0.0;
        vec3 specular = specFactor * object.specularColor.rgb * light.color.rgb * angularAttenuation * specMask;

        float spotShadow = 1.0;
        if (i < scene.spotShadowCount)
        {
            spotShadow = calculate_spot_shadow(i, scene.spotLightSpaceMatrices[i], fragPos);
        }

        finalColor += (diffuse + specular) * distAttenuation * light.color.a * spotShadow;
    }

    finalColor += emissiveColor;

    outColor = vec4(finalColor, 1.0);
}