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
    graphics::Material  material
) -> graphics::app::AppObject
{
    return graphics::app::AppObject{
        .name      = std::move(name),
        .mesh      = mesh,
        .transform = { .position = position, .rotation = rotation, .scale = scale },
        .material  = material,
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
    const auto* cube = &g_engine->cubeMesh;

    g_state.ambientLight = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    g_state.dirLightDir = glm::vec4(0.0f, -1.0f, -1.0f, 0.0f);
    g_state.dirLightColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    g_state.camera.position = { 3.8f,  2.4f,  3.8f };
    g_state.camera.yaw = -135.0f;
    g_state.camera.pitch = -12.0f;
    g_state.camera.fovy = glm::radians(70.0f);

    g_state.litObjects.push_back(make_plane(
        "Floor", plane,
        { 0.0f, 0.0f,  0.0f },
        { 0.0f, 0.0f,  0.0f },
        { 10.0f, 1.0f, 10.0f },
        make_material({ 0.20f, 0.13f, 0.08f, 1.0f },
                      { 0.55f, 0.45f, 0.35f, 1.0f },
                      96.0f)
    ));

    g_state.litObjects.push_back(make_plane(
        "Back Wall", plane,
        { 0.0f, 2.5f, -5.0f },
        { 90.0f, 0.0f, 0.0f },
        { 10.0f, 1.0f, 5.0f },
        make_material({ 0.32f, 0.28f, 0.26f, 1.0f },
                      { 0.10f, 0.10f, 0.10f, 1.0f },
                      8.0f)
    ));

    g_state.litObjects.push_back(make_plane(
        "Left Wall", plane,
        { -5.0f, 2.5f, 0.0f },
        { 0.0f, 0.0f, -90.0f },
        { 5.0f, 1.0f, 10.0f },
        make_material({ 0.42f, 0.34f, 0.28f, 1.0f },
                      { 0.10f, 0.10f, 0.10f, 1.0f },
                      8.0f)
    ));

    g_state.litObjects.push_back(make_plane(
        "Right Wall", plane,
        { 5.0f, 2.5f, 0.0f },
        { 0.0f, 0.0f, 90.0f },
        { 5.0f, 1.0f, 10.0f },
        make_material({ 0.28f, 0.30f, 0.34f, 1.0f },
                      { 0.10f, 0.10f, 0.10f, 1.0f },
                      8.0f)
    ));

    


    g_state.litObjects.push_back({
        .name = "Hero Cube",
        .mesh = cube,
        .transform = {
            .position = { -2.6f, 0.75f, -2.6f },
            .rotation = { 0.0f, 35.0f, 0.0f },
            .scale    = { 1.5f, 1.5f, 1.5f }
        },
        .material = make_material(
            { 0.85f, 0.62f, 0.30f, 1.0f },
            { 1.00f, 0.95f, 0.80f, 1.0f },
            128.0f)
    });

    g_state.litObjects.push_back({
        .name = "Spinner",
        .mesh = cube,
        .transform = {
            .position = { -1.0f, 0.35f, -1.0f },
            .rotation = { 0.0f, 0.0f, 0.0f },
            .scale    = { 0.7f, 0.7f, 0.7f }
        },
        .material = make_material(
            { 0.75f, 0.18f, 0.15f, 1.0f },
            { 1.00f, 0.90f, 0.90f, 1.0f },
            96.0f),
        .isAnimating = true,
        .animSpeed   = 0.8f,
        .animRadius  = 1.3f,
    });

    g_state.litObjects.push_back({
        .name = "Blue Cube",
        .mesh = cube,
        .transform = {
            .position = { 2.8f, 0.5f, -1.8f },
            .rotation = { 0.0f, 20.0f, 0.0f },
            .scale    = { 1.0f, 1.0f, 1.0f }
        },
        .material = make_material(
            { 0.18f, 0.32f, 0.65f, 1.0f },
            { 0.85f, 0.90f, 1.00f, 1.0f },
            96.0f)
    });

    g_state.litObjects.push_back(make_plane(
        "Painting", plane,
        { -2.5f, 2.6f, -4.97f },
        { 90.0f, 0.0f, 0.0f },
        { 2.2f, 1.0f, 1.6f },
        make_material({ 0.55f, 0.42f, 0.30f, 1.0f },
                      { 0.05f, 0.05f, 0.05f, 1.0f },
                      4.0f)
    ));

    

    g_state.ambientLight = { 0.03f, 0.03f, 0.05f, 1.0f };

    g_state.dirLightDir   = { 0.4f, -1.0f, 0.5f, 0.0f };
    g_state.dirLightColor = { 0.06f, 0.09f, 0.16f, 1.0f };

    

    g_state.spotLights.push_back({
        .position    = { -2.6f, 4.7f, -2.6f, 1.0f },
        .direction   = { 0.0f, -1.0f, 0.0f, 0.0f },
        .color       = { 1.00f, 0.80f, 0.45f, 1.5f },
        .innerCutoff = glm::cos(glm::radians(20.0f)),
        .outerCutoff = glm::cos(glm::radians(30.0f)),
        .linear      = 0.07f,
        .quadratic   = 0.017f
    });

    g_state.spotLights.push_back({
        .position    = { 0.5f, 3.5f, 0.5f, 1.0f },
        .direction   = glm::normalize(glm::vec4(-3.1f, -2.5f, -3.1f, 0.0f)),
        .color       = { 0.35f, 0.55f, 1.00f, 1.3f },
        .innerCutoff = glm::cos(glm::radians(15.0f)),
        .outerCutoff = glm::cos(glm::radians(25.0f)),
        .linear      = 0.09f,
        .quadratic   = 0.03f
    });

    g_state.spotLights.push_back({
        .position    = { -2.5f, 4.5f, -1.5f, 1.0f },
        .direction   = glm::normalize(glm::vec4(0.0f, -2.0f, -3.5f, 0.0f)),
        .color       = { 1.00f, 0.92f, 0.75f, 1.0f },
        .innerCutoff = glm::cos(glm::radians(22.0f)),
        .outerCutoff = glm::cos(glm::radians(32.0f)),
        .linear      = 0.05f,
        .quadratic   = 0.012f
    });

    g_state.spotLights.push_back({
        .position    = { 2.8f, 3.8f, 0.0f, 1.0f },
        .direction   = glm::normalize(glm::vec4(0.0f, -1.0f, -0.6f, 0.0f)),
        .color       = { 0.30f, 0.55f, 1.00f, 1.1f },
        .innerCutoff = glm::cos(glm::radians(18.0f)),
        .outerCutoff = glm::cos(glm::radians(28.0f)),
        .linear      = 0.09f,
        .quadratic   = 0.028f
    });

    g_state.pointLights.push_back({
        .position  = { 3.8f, 0.4f, 3.8f, 1.0f },
        .color     = { 1.00f, 0.42f, 0.12f, 1.1f },
        .linear    = 0.14f,
        .quadratic = 0.07f
    });

    g_state.pointLights.push_back({
        .position  = { -3.8f, 0.4f, 3.8f, 1.0f },
        .color     = { 0.15f, 0.42f, 1.00f, 1.0f },
        .linear    = 0.14f,
        .quadratic = 0.07f
    });

    g_state.pointLights.push_back({
        .position  = { 1.5f, 0.35f, -4.2f, 1.0f },
        .color     = { 0.85f, 0.20f, 0.55f, 1.0f },
        .linear    = 0.12f,
        .quadratic = 0.05f
    });

    g_state.pointLights.push_back({
        .position  = { -0.6f, 0.12f, -3.6f, 1.0f },
        .color     = { 1.00f, 0.70f, 0.35f, 0.9f },
        .linear    = 0.35f,
        .quadratic = 0.44f
    });

    g_engine->load_texture(
        std::filesystem::current_path() / "textures" / "wood.jpg",
        "Wood"
    );
    g_engine->load_texture(
        std::filesystem::current_path() / "textures" / "metal.jpg",
        "Metal"
    );

    g_state.litObjects[4].textureIndex = 1;


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
            obj.transform.position.y = std::sin(obj.animTime) * obj.animRadius;
            obj.transform.rotation.x += dt * 50.0f;
            obj.transform.rotation.z += dt * 50.0f;
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
            .textureDescriptorSet = g_engine->loadedTextures[obj.textureIndex].descriptorSet
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
                .textureDescriptorSet = g_engine->loadedTextures[0].descriptorSet
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
                .textureDescriptorSet = g_engine->loadedTextures[0].descriptorSet
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
                .textureDescriptorSet = g_engine->loadedTextures[0].descriptorSet
            });

            renderLineObjects.push_back({
                .mesh = &g_engine->coneMesh,
                .modelMatrix = innerModel,
                .normalMatrix = glm::mat4(1.0f),
                .material = lineMat,
                .textureDescriptorSet = g_engine->loadedTextures[0].descriptorSet
            });
        }
    }

    g_renderData.litObjects = renderLitObjects;
    g_renderData.unlitObjects = renderUnlitObjects;
    g_renderData.lineObjects = renderLineObjects;
    g_renderData.pointLights = g_state.pointLights;
    g_renderData.spotLights  = g_state.spotLights;

    g_engine->draw(fd.command_buffer, fd.framebuffer, g_renderData);
}

} // namespace graphics::app