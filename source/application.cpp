#include "application.hpp"
#include "common.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"
#include "graphics.hpp"
#include "graphics_internal.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>

namespace application
{
std::unique_ptr<graphics::core::VulkanEngine> g_engine;

class Camera
{
public:
    glm::vec3 position { 0.0f, 0.0f, 5.0f };
    float yaw { -90.0f };
    float pitch { 0.0f };
    float fovy { glm::radians(45.0f) };

    glm::mat4 get_view_matrix() const
    {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
        
        return glm::lookAt(position, position + front, glm::vec3(0, 1, 0));
    }
} g_camera;

struct AppObject
{
    std::string name;
    graphics::core::Transform transform;
    Material material;

    bool isAnimating = false;
    float animSpeed = 1.0f;
    float animRadius = 2.0f;
    float animTime = 0.0f;
};

struct AppState
{
    std::vector<PointLight> pointLights{};
    int projectionType{0}; // 0 - Perspective, 1 - Orthographic
    std::vector<AppObject> objects;
} g_state;


bool initialize()
{
    g_engine = std::make_unique<graphics::core::VulkanEngine>();
    g_engine->init();

    g_state.objects.push_back({
        .name = "Cube 1",
        .transform = { .position = {0.0f, 0.0f, -5.0f} },
        .material = {
            .albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .specularColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .shininess = 50.0f
        }
    });

    g_state.objects.push_back({
        .name = "Cube 2",
        .transform = { .position = {1.5f, 0.0f, -5.0f} },
        .material = {
            .albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .specularColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .shininess = 50.0f
        }
    });

    return true;
}


void shutdown()
{
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);
    g_engine->cleanup();
    g_engine.reset();
}

void update([[maybe_unused]] double time)
{
    static double lastTime = 0.0;
    float dt = static_cast<float>(time - lastTime);
    lastTime = time;

    ImGui::Begin("Lab 1: 3D Graphics");
    
    ImGui::Text("Projection");
    ImGui::RadioButton("Perspective", &g_state.projectionType, 0); ImGui::SameLine();
    ImGui::RadioButton("Orthographic", &g_state.projectionType, 1);
    
    int ui_id { 0 };
    for (auto& obj : g_state.objects)
    {
        ImGui::PushID(ui_id++);
        
        if (ImGui::CollapsingHeader((obj.name + " Controls").c_str()))
        {
            ImGui::SliderFloat3("Position", glm::value_ptr(obj.transform.position), -5.0f, 5.0f);
            ImGui::SliderFloat3("Rotation", glm::value_ptr(obj.transform.rotation), 0.0f, 360.0f);
            ImGui::SliderFloat3("Scale", glm::value_ptr(obj.transform.scale), 0.1f, 3.0f);
            
            ImGui::Separator();
            
            if (ImGui::Button(obj.isAnimating ? "Pause Animation" : "Play Animation"))
            {
                obj.isAnimating = !obj.isAnimating;
            }
            ImGui::SliderFloat("Speed", &obj.animSpeed, 0.1f, 5.0f);
            ImGui::SliderFloat("Radius", &obj.animRadius, 0.0f, 5.0f);
        }
        
        ImGui::PopID();
    }
    
    ImGui::End();

    for (auto& obj : g_state.objects)
    {
        if (obj.isAnimating)
        {
            obj.animTime += dt * obj.animSpeed;
            obj.transform.position.x = std::cos(obj.animTime) * obj.animRadius;
            obj.transform.position.y = std::sin(obj.animTime) * obj.animRadius;
            obj.transform.rotation.x += dt * 50.0f;
            obj.transform.rotation.y += dt * 50.0f;
        }
    }
}

void render(const graphics::internal::FrameData& fd)
{
    auto& context = graphics::internal::context;

    graphics::core::FrameRenderData renderData;
    renderData.sceneUniform = 
    {
        .view = g_camera.get_view_matrix(),
        .cameraPosition = glm::vec4(g_camera.position, 1.0f),
        .ambientLight = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f),
        .dirLightDir = glm::vec4(0.0f, -1.0f, 1.0f, 0.0f),
        .dirLightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .pointLightCount = 0,
        .spotLightCount = 0,
    };

    float aspect = static_cast<float>(context.swapchain_extent.width) / static_cast<float>(context.swapchain_extent.height);
    if (g_state.projectionType == 0)
    {
        renderData.sceneUniform.proj = glm::perspectiveZO(g_camera.fovy, aspect, 0.1f, 100.0f);
    }
    else
    {
        renderData.sceneUniform.proj = glm::orthoZO(-aspect * 2.0f, aspect * 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);
    }
    renderData.sceneUniform.proj[1][1] *= -1; // Y-axis for Vulkan

    std::vector<graphics::core::RenderObjectData> renderObjects;
    renderObjects.reserve(g_state.objects.size());
    
    for (const auto& obj : g_state.objects)
    {
        renderObjects.push_back({
            .modelMatrix = obj.transform.get_matrix(),
            .material = obj.material
        });
    }
    
    renderData.objects = renderObjects;

    g_engine->draw(fd.command_buffer, fd.framebuffer, renderData);
}

} // namespace application