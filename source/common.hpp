#pragma once

#include "gpu_resources.hpp"

#include "glm/ext/vector_float3.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <span>

struct Vertex final
{
    glm::vec3 position;
    glm::vec3 normal;
};

struct Material final
{
    glm::vec4 albedo;
    glm::vec4 specularColor;
    float shininess;
    float padding[3];
};

struct ObjectUniform final
{
    glm::mat4 model;
    glm::mat4 normalMatrix;
    Material material;
};



struct SceneUniform final
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



struct PointLight final
{
    glm::vec4 position;
    glm::vec4 color;
    float linear;
    float quadratic;
    glm::vec2 padding;
};

struct SpotLight final
{
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 color;
    float innerCutoff;
    float outerCutoff;
    float linear;
    float quadratic;
};



namespace graphics::core
{
    struct Mesh final
    {
        Buffer vertexBuffer;
        Buffer indexBuffer;
        uint32_t indexCount;
    };

    struct RenderObjectData final
    {
        const Mesh* mesh;
        glm::mat4 modelMatrix;
        glm::mat4 normalMatrix;
        Material material;
    };

    struct FrameRenderData final
    {
        SceneUniform sceneUniform;
        std::span<const PointLight> pointLights;
        std::span<const SpotLight> spotLights;
        std::span<const RenderObjectData> litObjects;
        std::span<const RenderObjectData> unlitObjects;
        std::span<const RenderObjectData> lineObjects;
    };


namespace detail
{

    inline constexpr std::array<Vertex, 24> vertices
    {{
        // Front - Normal: (0, 0, 1)
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 0
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 1
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 2
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 3

        // Back - Normal (0, 0, -1)
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}}, // 4
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}}, // 5
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}}, // 6
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}}, // 7

        // Left - Normal: (-1, 0, 0)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}}, // 8
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}}, // 9
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}}, // 10
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}}, // 11

        // Right - Normal: (1, 0, 0)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}}, // 12
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 13
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 14
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}}, // 15

        // Top - Normal: (0, 1, 0)
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}}, // 16
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}}, // 17
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 18
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 19

        // Bottom - Normal: (0, -1, 0)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}}, // 20
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}}, // 21
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}}, // 22
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}}  // 23
    }};

    inline constexpr std::array<uint32_t, 36> indices
    {
        0, 1, 2, 2, 3, 0,       // Front
        4, 5, 6, 6, 7, 4,       // Back
        8, 9, 10, 10, 11, 8,    // Left
        12, 13, 14, 14, 15, 12, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };

} // namespace graphics::core::detail
} // namespace graphics::core
