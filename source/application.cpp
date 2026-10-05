#include "application.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "graphics.hpp"
#include "graphics_internal.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>

namespace application
{
std::unique_ptr<graphics::core::VulkanEngine> engine;

struct AppObject
{
    std::string name;
    graphics::core::Transform transform;
    glm::vec3 color{1.0f, 1.0f, 1.0f};

    bool isAnimating = false;
    float animSpeed = 1.0f;
    float animRadius = 2.0f;
    float animTime = 0.0f;
};

struct AppState
{
    int projectionType{0}; // 0 - Perspective, 1 - Orthographic
    std::vector<AppObject> objects;
} state;


bool initialize()
{
    engine = std::make_unique<graphics::core::VulkanEngine>();
    engine->init();

    state.objects.push_back({
        .name = "Cube 1",
        .transform = { .position = {0.0f, 0.0f, -5.0f} },
        .color = {1.0f, 1.0f, 1.0f}
    });

    state.objects.push_back({
        .name = "Cube 2",
        .transform = { .position = {1.5f, 0.0f, -5.0f} },
        .color = {0.2f, 0.5f, 1.0f}
    });

    return true;
}


void shutdown()
{
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);
    engine->cleanup();
    engine.reset();
}

void update([[maybe_unused]] double time)
{
    static double lastTime = 0.0;
    float dt = static_cast<float>(time - lastTime);
    lastTime = time;

    ImGui::Begin("Lab 1: 3D Graphics");
    
    ImGui::Text("Projection");
    ImGui::RadioButton("Perspective", &state.projectionType, 0); ImGui::SameLine();
    ImGui::RadioButton("Orthographic", &state.projectionType, 1);
    
    int ui_id { 0 };
    for (auto& obj : state.objects)
    {
        ImGui::PushID(ui_id++);
        
        if (ImGui::CollapsingHeader((obj.name + " Controls").c_str()))
        {
            ImGui::SliderFloat3("Position", glm::value_ptr(obj.transform.position), -5.0f, 5.0f);
            ImGui::SliderFloat3("Rotation", glm::value_ptr(obj.transform.rotation), 0.0f, 360.0f);
            ImGui::SliderFloat3("Scale", glm::value_ptr(obj.transform.scale), 0.1f, 3.0f);
            ImGui::ColorEdit3("Color", glm::value_ptr(obj.color));
            
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

    for (auto& obj : state.objects)
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
    renderData.view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    
    float aspect = static_cast<float>(context.swapchain_extent.width) / static_cast<float>(context.swapchain_extent.height);
    if (state.projectionType == 0)
    {
        renderData.proj = glm::perspectiveZO(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    }
    else
    {
        renderData.proj = glm::orthoZO(-aspect * 2.0f, aspect * 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);
    }
    renderData.proj[1][1] *= -1; // Y-axis for Vulkan

    std::vector<graphics::core::RenderObjectData> renderObjects;
    renderObjects.reserve(state.objects.size());
    
    for (const auto& obj : state.objects)
    {
        renderObjects.push_back({
            .modelMatrix = obj.transform.get_matrix(),
            .color = glm::vec4(obj.color, 1.0f)
        });
    }
    
    renderData.objects = renderObjects;

    engine->draw(fd.command_buffer, fd.framebuffer, renderData);
}

} // namespace application