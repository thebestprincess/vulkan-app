#pragma once

#include "gpu_resources.hpp"

#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <span>

namespace graphics
{

struct Vertex final
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
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

struct PushConstants final
{
    uint32_t isUnlit;
    float time;
    uint32_t enableComplexSampling;
};

namespace consts
{
    // number of objects on the scene simultaneously
    static constexpr size_t maxObjects { 1000uz }; 

    static constexpr size_t maxPointLights { 100uz };
    static constexpr size_t maxSpotLights { 100uz };

} // namespace::consts 


namespace core
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
    VkDescriptorSet textureDescriptorSet;
};

struct FrameRenderData final
{
    SceneUniform sceneUniform;
    std::span<const PointLight> pointLights;
    std::span<const SpotLight> spotLights;
    std::span<const RenderObjectData> litObjects;
    std::span<const RenderObjectData> unlitObjects;
    std::span<const RenderObjectData> lineObjects;
    double time;
};

struct TextureResource final
{
    std::string name;
    Texture texture;
    VkDescriptorSet descriptorSet { VK_NULL_HANDLE };
};


namespace detail
{

    inline constexpr std::array<Vertex, 24> vertices
    {{
        // Front - Normal: (0, 0, 1)
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 0 (Bottom-left)
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 1 (Bottom-right)
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // 2 (Top-right)
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // 3 (Top-left)

        // Back - Normal (0, 0, -1)
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}}, // 4
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}}, // 5
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}}, // 6
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // 7

        // Left - Normal: (-1, 0, 0)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 8
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 9
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 10
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 11

        // Right - Normal: (1, 0, 0)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 12
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 13
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 14
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 15

        // Top - Normal: (0, 1, 0)
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // 16
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // 17
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // 18
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, // 19

        // Bottom - Normal: (0, -1, 0)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}, // 20
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}}, // 21
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // 22
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}  // 23
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

namespace app
{

class Camera
{
public:
    glm::vec3 position { 0.0f, 0.0f, 5.0f };
    float yaw { -90.0f };
    float pitch { 0.0f };
    float fovy { glm::radians(45.0f) };
    float movement_speed { 5.0f };
    float turn_speed { 60.0f };

    glm::mat4 get_view_matrix() const
    {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
        
        return glm::lookAt(position, position + front, glm::vec3(0, 1, 0));
    }
};

struct Transform final
{
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 get_matrix() const
    {
        glm::mat4 mat = glm::translate(glm::mat4(1.0f), position);
        mat = glm::rotate(mat, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        mat = glm::rotate(mat, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        mat = glm::rotate(mat, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        mat = glm::scale(mat, scale);
        return mat;
    }
    glm::mat4 get_normal_matrix() const
    {
        glm::mat4 mat = get_matrix();
        return glm::transpose(glm::inverse(mat));
    }
};

struct AppObject
{
    std::string name;
    const core::Mesh* mesh;
    Transform transform;
    Material material;
    size_t textureIndex{0};

    bool isAnimating = false;
    float animSpeed = 1.0f;
    float animRadius = 2.0f;
    float animTime = 0.0f;
};

struct AppState
{
    Camera camera{};

    glm::vec4 ambientLight{};
    glm::vec4 dirLightDir{};
    glm::vec4 dirLightColor{};

    std::vector<PointLight> pointLights{};
    std::vector<SpotLight> spotLights{};

    int projectionType{0}; // 0 - Perspective, 1 - Orthographic
    std::vector<AppObject> litObjects;
    std::vector<AppObject> unlitObjects;
    std::vector<AppObject> lineObjects;

    bool show_point_lights { true }; 
    bool show_spot_lights { true };
};

} // namespace graphics::app

} // namespace graphics