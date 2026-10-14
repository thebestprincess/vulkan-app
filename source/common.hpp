#pragma once

#include "glm/ext/vector_float3.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

struct SceneUniform
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPosition;
    glm::vec4 ambientLight;

    glm::vec4 dirLightDir;
    glm::vec4 dirLightColor;
    
    uint32_t pointLightCount;
    uint32_t spotLightCount;
    glm::vec2 padding;
};

struct Material
{
    glm::vec4 albedo;
    glm::vec4 specularColor;
    float shininess;
    float padding[3];
};

struct ObjectUniform
{
    glm::mat4 model;
    Material material;
};



struct PointLight
{
    glm::vec4 position;
    glm::vec4 color;
};

struct SpotLight
{
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 color;
    glm::vec2 angles;
    glm::vec2 padding;
};