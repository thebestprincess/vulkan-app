#include "application.hpp"
#include "common.hpp"
#include "engine.hpp"
#include "graphics_internal.hpp"
#include "UI.hpp"

#include <GLFW/glfw3.h>
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <filesystem>
#include <memory>
#include <algorithm>

namespace graphics::app
{

auto make_material(
    glm::vec4 albedo,
    glm::vec4 specular,
    float shininess
) -> graphics::Material
{
    return graphics::Material{
        .albedo        = albedo,
        .specularColor = specular,
        .shininess     = shininess,
    };
}

auto make_plane(
    std::string name,
    const graphics::core::Mesh* mesh,
    glm::vec3 position,
    glm::vec3 rotation,
    glm::vec3 scale,
    graphics::Material material,
    size_t materialIndex = 0
) -> graphics::app::AppObject
{
    return graphics::app::AppObject{
        .name          = std::move(name),
        .mesh          = mesh,
        .transform     = { .position = position, .rotation = rotation, .scale = scale },
        .material      = material,
        .materialIndex = materialIndex,
    };
}

auto make_box(
    std::string name,
    const graphics::core::Mesh* mesh,
    glm::vec3 position,
    glm::vec3 rotation,
    glm::vec3 scale,
    graphics::Material material,
    size_t materialIndex = 0
) -> graphics::app::AppObject
{
    return graphics::app::AppObject{
        .name          = std::move(name),
        .mesh          = mesh,
        .transform     = { .position = position, .rotation = rotation, .scale = scale },
        .material      = material,
        .materialIndex = materialIndex,
    };
}

} // namespace


namespace graphics::app
{
std::unique_ptr<core::VulkanEngine> g_engine;
AppState g_state;
core::FrameRenderData g_renderData;

bool initialize()
{
    g_engine = std::make_unique<core::VulkanEngine>();
    g_engine->init();

    const auto* plane = &g_engine->planeMesh;
    const auto* cube  = &g_engine->cubeMesh;

    const size_t matDefault = g_engine->load_material("Default", "", "", "");
    const size_t matMetal = g_engine->load_material(
        "Metal",
        std::filesystem::current_path() / "textures" / "metal.jpg",
        std::filesystem::current_path() / "textures" / "metal_specular.png",
        std::filesystem::current_path() / "textures" / "metal_emissive.png"
    );
    const size_t matRustMetal = g_engine->load_material(
        "Rust Metal",
        std::filesystem::current_path() / "textures" / "rust_metal.png",
        std::filesystem::current_path() / "textures" / "rust_metal_specular.png",
        ""
    );
    const size_t matLava = g_engine->load_material(
        "Lava",
        std::filesystem::current_path() / "textures" / "lava.png",
        std::filesystem::current_path() / "textures" / "lava_specular.png",
        std::filesystem::current_path() / "textures" / "lava_emissive.png"
    );
    const size_t matWood  = g_engine->load_material(
        "Wood",
        std::filesystem::current_path() / "textures" / "wood.jpg",
        "",
        ""
        // std::filesystem::current_path() / "textures" / "wood_emissive.jpg"
    );
    const size_t matAhegao = g_engine->load_material(
        "Ahegao",
        std::filesystem::current_path() / "textures" / "ahegao.jpg",
        "",
        std::filesystem::current_path() / "textures" / "ahegao_emissive.png"
    );

    g_state.camera.position = { 4.8f, 3.3f, 6.2f };
    g_state.camera.yaw      = -126.0f;
    g_state.camera.pitch    = -17.0f;
    g_state.camera.fovy     = glm::radians(65.0f);

    g_state.shadowSettings.orthoSize         = 12.0f;
    g_state.shadowSettings.lightDistance     = 14.0f;
    g_state.shadowSettings.nearPlane         = 0.1f;
    g_state.shadowSettings.farPlane          = 35.0f;
    g_state.shadowSettings.depthBiasConstant = 1.25f;
    g_state.shadowSettings.depthBiasSlope    = 1.75f;

    g_state.litObjects.push_back(make_plane(
        "Gallery Floor", plane,
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 12.0f, 1.0f, 12.0f },
        make_material({ 0.06f, 0.05f, 0.05f, 1.0f }, { 0.65f, 0.55f, 0.45f, 1.0f }, 96.0f),
        matDefault
    ));

    g_state.litObjects.push_back(make_plane(
        "Back Wall", plane,
        { 0.0f, 3.0f, -6.0f },
        { 90.0f, 0.0f, 0.0f },
        { 12.0f, 1.0f, 6.0f },
        make_material({ 0.35f, 0.33f, 0.32f, 1.0f }, { 0.12f, 0.12f, 0.12f, 1.0f }, 12.0f),
        matDefault
    ));

    g_state.litObjects.push_back(make_plane(
        "Left Wall", plane,
        { -6.0f, 3.0f, 0.0f },
        { 0.0f, 0.0f, -90.0f },
        { 6.0f, 1.0f, 12.0f },
        make_material({ 0.38f, 0.34f, 0.32f, 1.0f }, { 0.12f, 0.12f, 0.12f, 1.0f }, 12.0f),
        matDefault
    ));

    g_state.litObjects.push_back(make_plane(
        "Right Wall", plane,
        { 6.0f, 3.0f, 0.0f },
        { 0.0f, 0.0f, 90.0f },
        { 6.0f, 1.0f, 12.0f },
        make_material({ 0.30f, 0.32f, 0.36f, 1.0f }, { 0.12f, 0.12f, 0.12f, 1.0f }, 12.0f),
        matDefault
    ));

    g_state.litObjects.push_back(make_box(
        "Left Pillar", cube,
        { -5.2f, 3.0f, -5.2f },
        { 0.0f, 0.0f, 0.0f },
        { 0.8f, 6.0f, 0.8f },
        make_material({ 0.55f, 0.52f, 0.48f, 1.0f }, { 0.30f, 0.30f, 0.30f, 1.0f }, 32.0f),
        matRustMetal
    ));

    g_state.litObjects.push_back(make_box(
        "Right Pillar", cube,
        { 5.2f, 3.0f, -5.2f },
        { 0.0f, 0.0f, 0.0f },
        { 0.8f, 6.0f, 0.8f },
        make_material({ 0.55f, 0.52f, 0.48f, 1.0f }, { 0.30f, 0.30f, 0.30f, 1.0f }, 32.0f),
        matRustMetal
    ));

    g_state.litObjects.push_back(make_box(
        "Center Pedestal", cube,
        { 0.0f, 0.5f, 0.0f },
        { 0.0f, 0.0f, 0.0f },
        { 1.7f, 1.0f, 1.7f },
        make_material({ 0.65f, 0.62f, 0.58f, 1.0f }, { 0.45f, 0.45f, 0.45f, 1.0f }, 48.0f),
        matRustMetal
    ));

    g_state.litObjects.push_back(make_box(
        "Centerpiece Cube", cube,
        { 0.0f, 1.65f, 0.0f },
        { 0.0f, 35.0f, 0.0f },
        { 1.3f, 1.3f, 1.3f },
        make_material({ 0.92f, 0.86f, 0.78f, 1.0f }, { 1.00f, 0.96f, 0.88f, 1.0f }, 128.0f),
        matMetal
    ));

    g_state.litObjects.push_back({
        .name = "Orbiting Cube",
        .mesh = cube,
        .transform = {
            .position = { 2.0f, 2.2f, 0.0f },
            .rotation = { 15.0f, 0.0f, 25.0f },
            .scale    = { 0.55f, 0.55f, 0.55f }
        },
        .material = make_material(
            { 0.85f, 0.28f, 0.22f, 1.0f },
            { 1.00f, 0.85f, 0.80f, 1.0f },
            96.0f
        ),
        .materialIndex = matDefault,
        .isAnimating   = true,
        .animSpeed     = 0.75f,
        .animRadius    = 2.1f,
    });

    g_state.litObjects.push_back(make_box(
        "Left Pedestal", cube,
        { -3.6f, 0.45f, -1.6f },
        { 0.0f, 15.0f, 0.0f },
        { 1.2f, 0.9f, 1.2f },
        make_material({ 0.45f, 0.42f, 0.40f, 1.0f }, { 0.30f, 0.30f, 0.30f, 1.0f }, 32.0f),
        matRustMetal
    ));

    g_state.litObjects.push_back(make_box(
        "Lava Artifact", cube,
        { -3.6f, 1.35f, -1.6f },
        { 15.0f, 40.0f, 10.0f },
        { 0.85f, 0.85f, 0.85f },
        make_material({ 0.95f, 0.55f, 0.35f, 1.0f }, { 0.80f, 0.50f, 0.30f, 1.0f }, 64.0f),
        matLava
    ));

    g_state.litObjects.push_back(make_box(
        "Right Pedestal", cube,
        { 3.6f, 0.45f, -1.6f },
        { 0.0f, -15.0f, 0.0f },
        { 1.2f, 0.9f, 1.2f },
        make_material({ 0.45f, 0.42f, 0.40f, 1.0f }, { 0.30f, 0.30f, 0.30f, 1.0f }, 32.0f),
        matRustMetal
    ));

    g_state.litObjects.push_back(make_box(
        "Azure Sculpture", cube,
        { 3.6f, 1.40f, -1.6f },
        { 25.0f, -30.0f, 35.0f },
        { 0.90f, 0.90f, 0.90f },
        make_material({ 0.25f, 0.55f, 0.95f, 1.0f }, { 0.90f, 0.95f, 1.00f, 1.0f }, 128.0f),
        matMetal
    ));

    g_state.litObjects.push_back(make_box(
        "Frame Left", cube,
        { -2.4f, 2.9f, -5.96f },
        { 0.0f, 0.0f, 0.0f },
        { 2.35f, 1.75f, 0.06f },
        make_material({ 0.20f, 0.14f, 0.10f, 1.0f }, { 0.40f, 0.30f, 0.20f, 1.0f }, 32.0f),
        matWood
    ));
    g_state.litObjects.push_back(make_plane(
        "Painting Left", plane,
        { -2.4f, 2.9f, -5.91f },
        { 90.0f, 0.0f, 0.0f },
        { 2.1f, 1.0f, 1.5f },
        make_material({ 0.95f, 0.95f, 0.95f, 1.0f }, { 0.10f, 0.10f, 0.10f, 1.0f }, 8.0f),
        matAhegao
    ));

    g_state.litObjects.push_back(make_box(
        "Frame Right", cube,
        { 2.4f, 2.9f, -5.96f },
        { 0.0f, 0.0f, 0.0f },
        { 2.35f, 1.75f, 0.06f },
        make_material({ 0.20f, 0.14f, 0.10f, 1.0f }, { 0.40f, 0.30f, 0.20f, 1.0f }, 32.0f),
        matWood
    ));
    g_state.litObjects.push_back(make_plane(
        "Painting Right", plane,
        { 2.4f, 2.9f, -5.91f },
        { 90.0f, 0.0f, 0.0f },
        { 2.1f, 1.0f, 1.5f },
        make_material({ 0.95f, 0.85f, 0.80f, 1.0f }, { 0.20f, 0.20f, 0.20f, 1.0f }, 16.0f),
        matLava
    ));

    g_state.litObjects.push_back(make_box(
        "Frame Side", cube,
        { -5.96f, 2.9f, 0.8f },
        { 0.0f, 0.0f, 0.0f },
        { 0.06f, 1.95f, 2.55f },
        make_material({ 0.20f, 0.14f, 0.10f, 1.0f }, { 0.40f, 0.30f, 0.20f, 1.0f }, 32.0f),
        matWood
    ));
    g_state.litObjects.push_back(make_plane(
        "Painting Side", plane,
        { -5.91f, 2.9f, 0.8f },
        { 0.0f, 0.0f, -90.0f },
        { 1.7f, 1.0f, 2.3f },
        make_material({ 0.90f, 0.92f, 0.98f, 1.0f }, { 0.40f, 0.40f, 0.40f, 1.0f }, 32.0f),
        matMetal
    ));

    g_state.ambientLight  = { 0.04f, 0.04f, 0.06f, 1.0f };
    g_state.dirLightDir   = { -0.25f, -0.85f, -0.55f, 0.0f };
    g_state.dirLightColor = { 0.95f, 0.90f, 0.82f, 0.38f };

    const glm::vec3 centerTarget { 0.0f, 1.55f, 0.0f };

    {
        const glm::vec3 pos { -3.6f, 3.5f, 3.4f };
        g_state.spotLights.push_back({
            .position    = glm::vec4(pos, 1.0f),
            .direction   = glm::vec4(glm::normalize(centerTarget - pos), 0.0f),
            .color       = { 1.00f, 0.78f, 0.42f, 1.6f },
            .innerCutoff = glm::cos(glm::radians(16.0f)),
            .outerCutoff = glm::cos(glm::radians(26.0f)),
            .linear      = 0.06f,
            .quadratic   = 0.015f
        });
    }

    {
        const glm::vec3 pos { 3.6f, 3.3f, 3.4f };
        g_state.spotLights.push_back({
            .position    = glm::vec4(pos, 1.0f),
            .direction   = glm::vec4(glm::normalize(centerTarget - pos), 0.0f),
            .color       = { 0.30f, 0.65f, 1.00f, 1.5f },
            .innerCutoff = glm::cos(glm::radians(16.0f)),
            .outerCutoff = glm::cos(glm::radians(26.0f)),
            .linear      = 0.06f,
            .quadratic   = 0.015f
        });
    }

    {
        const glm::vec3 pos { -4.2f, 2.9f, -3.2f };
        g_state.spotLights.push_back({
            .position    = glm::vec4(pos, 1.0f),
            .direction   = glm::vec4(glm::normalize(centerTarget - pos), 0.0f),
            .color       = { 0.85f, 0.30f, 0.90f, 1.3f },
            .innerCutoff = glm::cos(glm::radians(15.0f)),
            .outerCutoff = glm::cos(glm::radians(25.0f)),
            .linear      = 0.07f,
            .quadratic   = 0.018f
        });
    }

    {
        const glm::vec3 pos { 0.0f, 4.4f, -1.8f };
        const glm::vec3 target { 0.0f, 2.7f, -5.9f };
        g_state.spotLights.push_back({
            .position    = glm::vec4(pos, 1.0f),
            .direction   = glm::vec4(glm::normalize(target - pos), 0.0f),
            .color       = { 1.00f, 0.92f, 0.80f, 1.2f },
            .innerCutoff = glm::cos(glm::radians(28.0f)),
            .outerCutoff = glm::cos(glm::radians(40.0f)),
            .linear      = 0.05f,
            .quadratic   = 0.012f
        });
    }

    g_state.pointLights.push_back({
        .position  = { -3.6f, 2.2f, -1.6f, 1.0f },
        .color     = { 1.00f, 0.42f, 0.12f, 1.1f },
        .linear    = 0.22f,
        .quadratic = 0.20f
    });

    g_state.pointLights.push_back({
        .position  = { 3.6f, 2.2f, -1.6f, 1.0f },
        .color     = { 0.20f, 0.58f, 1.00f, 1.0f },
        .linear    = 0.22f,
        .quadratic = 0.20f
    });

    g_state.pointLights.push_back({
        .position  = { -4.4f, 3.6f, -4.8f, 1.0f },
        .color     = { 1.00f, 0.78f, 0.45f, 0.9f },
        .linear    = 0.20f,
        .quadratic = 0.16f
    });

    g_state.pointLights.push_back({
        .position  = { 4.4f, 3.6f, -4.8f, 1.0f },
        .color     = { 1.00f, 0.78f, 0.45f, 0.9f },
        .linear    = 0.20f,
        .quadratic = 0.16f
    });

    g_state.pointLights.push_back({
        .position  = { -4.8f, 0.1f, 4.8f, 1.0f },
        .color     = { 0.48f, 0.32f, 0.95f, 0.75f },
        .linear    = 0.22f,
        .quadratic = 0.18f
    });

    g_state.pointLights.push_back({
        .position  = { 4.8f, 0.1f, 4.8f, 1.0f },
        .color     = { 0.72f, 0.16f, 0.09f, 0.75f },
        .linear    = 0.22f,
        .quadratic = 0.18f
    });

    return true;
}


void shutdown()
{
	vkQueueWaitIdle(internal::context.graphics_queue);
    g_engine.reset();
}

void update(double time)
{
    static double lastTime = 0.0;
    float dt = static_cast<float>(time - lastTime);
    lastTime = time;
    g_renderData.time = time;

    ui::camera_motion(g_state, dt);
    ui::update_imgui(g_state, *g_engine);

    for (auto& obj : g_state.litObjects)
    {
        if (obj.isAnimating)
        {
            obj.animTime += dt * obj.animSpeed;
            obj.transform.position.x = std::cos(obj.animTime) * obj.animRadius;
            obj.transform.position.z = std::sin(obj.animTime) * obj.animRadius;
            obj.transform.position.y = 2.2f + 0.25f * std::sin(obj.animTime * 2.0f);
            obj.transform.rotation.x += dt * 45.0f;
            obj.transform.rotation.y += dt * 60.0f;
        }
    }
}

void render(const internal::FrameData& fd)
{
    auto& context = internal::context;

    g_renderData.sceneUniform = 
    {
        .view = g_state.camera.get_view_matrix(),
        .cameraPosition = glm::vec4(g_state.camera.position, 1.0f),
        .ambientLight = glm::vec4(g_state.ambientLight),
        .dirLightDir = glm::vec4(g_state.dirLightDir),
        .dirLightColor = glm::vec4(g_state.dirLightColor),
        .pointLightCount = g_state.show_point_lights ? static_cast<uint32_t>(g_state.pointLights.size()) : static_cast<uint32_t>(0),
        .spotLightCount = g_state.show_spot_lights ? static_cast<uint32_t>(g_state.spotLights.size()) : static_cast<uint32_t>(0),
    };

    glm::vec3 dir = glm::vec3(g_state.dirLightDir);
    if (glm::length(dir) < 1e-4f)
    {
        dir = glm::vec3(0.0f, -1.0f, 0.0f);
    }
    dir = glm::normalize(dir);

    const glm::vec3 sunPos = -dir * g_state.shadowSettings.lightDistance;
    const glm::vec3 sunUp = (std::abs(dir.y) > 0.99f)
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 dirView = glm::lookAt(sunPos, glm::vec3(0.0f), sunUp);

    const float orthoHalf = g_state.shadowSettings.orthoSize;
    glm::mat4 dirProj = glm::orthoZO(
        -orthoHalf, orthoHalf,
        -orthoHalf, orthoHalf,
        g_state.shadowSettings.nearPlane,
        g_state.shadowSettings.farPlane
    );
    dirProj[1][1] *= -1.0f; // Y-axis for Vulkan
    g_state.shadowSettings.dirLightSpaceMatrix = dirProj * dirView;

    uint32_t spotShadowCount { 0u };
    if (g_state.show_spot_lights && g_state.shadowSettings.enableSpotShadows)
    {
        spotShadowCount = static_cast<uint32_t>(
            std::min(g_state.spotLights.size(), consts::maxSpotShadowLights)
        );

        for (uint32_t i = 0; i < spotShadowCount; ++i)
        {
            const auto& spot = g_state.spotLights[i];
            const glm::vec3 spotPos = glm::vec3(spot.position);
            glm::vec3 spotDir = glm::vec3(spot.direction);
            if (glm::length(spotDir) < 1e-4f)
            {
                spotDir = glm::vec3(0.0f, -1.0f, 0.0f);
            }
            spotDir = glm::normalize(spotDir);

            const glm::vec3 spotUp = (std::abs(spotDir.y) < 0.99f)
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(0.0f, 0.0f, 1.0f);

            const glm::mat4 spotView = glm::lookAt(spotPos, spotPos + spotDir, spotUp);
            const float spotFov = 2.0f * std::acos(std::clamp(spot.outerCutoff, 0.01f, 0.9999f));
            glm::mat4 spotProj = glm::perspectiveZO(
                spotFov,
                1.0f,
                0.1f,
                g_state.shadowSettings.farPlane
            );
            spotProj[1][1] *= -1.0f; // Y-axis for Vulkan
            g_state.shadowSettings.spotLightSpaceMatrices[i] = spotProj * spotView;
        }
    }

    g_renderData.sceneUniform.dirLightSpaceMatrix = g_state.shadowSettings.dirLightSpaceMatrix;
    g_renderData.sceneUniform.spotLightSpaceMatrices = g_state.shadowSettings.spotLightSpaceMatrices;
    g_renderData.sceneUniform.spotShadowCount = spotShadowCount;

    float aspect = static_cast<float>(context.swapchain_extent.width) / static_cast<float>(context.swapchain_extent.height);
    if (g_state.projectionType == 0)
    {
        g_renderData.sceneUniform.proj = glm::perspectiveZO(g_state.camera.fovy, aspect, 0.1f, 100.0f);
    }
    else
    {
        g_renderData.sceneUniform.proj = glm::orthoZO(-aspect * 2.0f, aspect * 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);
    }
    g_renderData.sceneUniform.proj[1][1] *= -1; // Y-axis for Vulkan

    std::vector<core::RenderObjectData> renderLitObjects;
    renderLitObjects.reserve(g_state.litObjects.size());
    
    for (const auto& obj : g_state.litObjects)
    {
        renderLitObjects.push_back({
            .mesh  = obj.mesh,
            .modelMatrix = obj.transform.get_matrix(),
            .normalMatrix = obj.transform.get_normal_matrix(),
            .material = obj.material,
            .materialDescriptorSet = g_engine->loadedMaterials[obj.materialIndex].descriptorSet,
            .useComplexSampling = obj.useComplexSampling
        });
    }

    std::vector<core::RenderObjectData> renderUnlitObjects;
    
    if (g_state.show_point_lights)
    {
        renderUnlitObjects.reserve(g_state.unlitObjects.size());

        for (const auto& light : g_state.pointLights)
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(light.position));
            model = glm::scale(model, glm::vec3(0.2f));

            Material unlitMat{};
            unlitMat.albedo = light.color;

            renderUnlitObjects.push_back({
                .mesh = &g_engine->cubeMesh,
                .modelMatrix = model,
                .normalMatrix = glm::mat4(1.0f),
                .material = unlitMat,
                .materialDescriptorSet = g_engine->loadedMaterials[0].descriptorSet
            });
        }
    }

    std::vector<core::RenderObjectData> renderLineObjects;
    if (g_state.show_spot_lights)
    {
        for (const auto& light : g_state.spotLights)
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(light.position));
            model = glm::scale(model, glm::vec3(0.2f));

            Material unlitMat{};
            unlitMat.albedo = light.color;

            renderUnlitObjects.push_back({
                .mesh = &g_engine->cubeMesh,
                .modelMatrix = model,
                .normalMatrix = glm::mat4(1.0f),
                .material = unlitMat,
                .materialDescriptorSet = g_engine->loadedMaterials[0].descriptorSet
            });
        }
        
        for (const auto& light : g_state.spotLights)
        {
            glm::vec3 pos = glm::vec3(light.position);
            glm::vec3 dir = glm::normalize(glm::vec3(light.direction));

            glm::vec3 up = std::abs(dir.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            glm::mat4 model = glm::inverse(glm::lookAt(pos, pos + dir, up));

            float outerAngle = std::acos(light.outerCutoff);
            float outerLength = 3.0f;
            float outerRadius = outerLength * std::tan(outerAngle); 

            float innerAngle = std::acos(light.innerCutoff);
            float innerLength = 3.0f;
            float innerRadius = innerLength * std::tan(innerAngle);
            
            glm::mat4 outerModel = glm::scale(model, glm::vec3(outerRadius, outerRadius, outerLength));
            glm::mat4 innerModel = glm::scale(model, glm::vec3(innerRadius, innerRadius, innerLength));

            Material lineMat{};
            lineMat.albedo = light.color;

            renderLineObjects.push_back({
                .mesh = &g_engine->coneMesh,
                .modelMatrix = outerModel,
                .normalMatrix = glm::mat4(1.0f),
                .material = lineMat,
                .materialDescriptorSet = g_engine->loadedMaterials[0].descriptorSet
            });

            renderLineObjects.push_back({
                .mesh = &g_engine->coneMesh,
                .modelMatrix = innerModel,
                .normalMatrix = glm::mat4(1.0f),
                .material = lineMat,
                .materialDescriptorSet = g_engine->loadedMaterials[0].descriptorSet
            });
        }
    }

    g_renderData.litObjects = renderLitObjects;
    g_renderData.unlitObjects = renderUnlitObjects;
    g_renderData.lineObjects = renderLineObjects;
    g_renderData.pointLights = g_state.pointLights;
    g_renderData.spotLights  = g_state.spotLights;
    g_renderData.shadowSettings = g_state.shadowSettings;

    g_engine->draw(fd.command_buffer, fd.framebuffer, g_renderData);
}

} // namespace graphics::app