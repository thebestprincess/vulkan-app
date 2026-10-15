#include "application.hpp"
#include "common.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"
#include "engine.hpp"
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
    const graphics::core::Mesh* mesh;
    Transform transform;
    Material material;

    bool isAnimating = false;
    float animSpeed = 1.0f;
    float animRadius = 2.0f;
    float animTime = 0.0f;
};

struct AppState
{
    Camera camera{};
    std::vector<PointLight> pointLights{};
    std::vector<SpotLight> spotLights{};
    int projectionType{0}; // 0 - Perspective, 1 - Orthographic
    std::vector<AppObject> litObjects;
    std::vector<AppObject> unlitObjects;
    std::vector<AppObject> lineObjects;
} g_state;


bool initialize()
{
    g_engine = std::make_unique<graphics::core::VulkanEngine>();
    g_engine->init();

    g_state.litObjects.push_back({
        .name = "Cube 1",
        .mesh = &g_engine->cubeMesh,
        .transform = { .position = {0.0f, 0.0f, -5.0f} },
        .material = {
            .albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .specularColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .shininess = 50.0f
        }
    });

    g_state.litObjects.push_back({
        .name = "Cube 2",
        .mesh = &g_engine->cubeMesh,
        .transform = { .position = {1.5f, 0.0f, -5.0f} },
        .material = {
            .albedo = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
            .specularColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .shininess = 50.0f
        }
    });

    g_state.spotLights.push_back({
        .position = glm::vec4(0.0f, 2.0f, 0.0f, 1.0f),
        .direction = glm::vec4(0.0f, -1.0f, 0.0f, 1.0f),
        .color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
        .innerCutoff = glm::cos(glm::radians(70.0f)),
        .outerCutoff = glm::cos(glm::radians(30.0f)),
        .linear = 0.045,
        .quadratic = 0.0075,
    });

    g_state.pointLights.push_back({
        .position = glm::vec4(-1.5f, 0.0f, -5.0f, 1.0f),
        .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .linear = 0.22,
        .quadratic = 0.2,
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

    ImGui::Begin("Lab 2: 3D Graphics");
    
    ImGui::Text("Projection");
    ImGui::RadioButton("Perspective", &g_state.projectionType, 0); ImGui::SameLine();
    ImGui::RadioButton("Orthographic", &g_state.projectionType, 1);
    
    int ui_id { 0 };
    for (auto& obj : g_state.litObjects)
    {
        ImGui::PushID(ui_id++);
        
        if (ImGui::CollapsingHeader((obj.name + " Controls").c_str()))
        {
            ImGui::SliderFloat3("Position", glm::value_ptr(obj.transform.position), -5.0f, 5.0f);
            ImGui::SliderFloat3("Rotation", glm::value_ptr(obj.transform.rotation), 0.0f, 360.0f);
            ImGui::SliderFloat3("Scale", glm::value_ptr(obj.transform.scale), 0.1f, 3.0f);
            ImGui::SliderFloat3("Color", glm::value_ptr(obj.transform.scale), 0.1f, 3.0f);
            
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

    for (auto& obj : g_state.litObjects)
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
        .view = g_state.camera.get_view_matrix(),
        .cameraPosition = glm::vec4(g_state.camera.position, 1.0f),
        .ambientLight = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f),
        .dirLightDir = glm::vec4(0.0f, -1.0f, 1.0f, 0.0f),
        .dirLightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        .pointLightCount = static_cast<uint32_t>(g_state.pointLights.size()),
        .spotLightCount = static_cast<uint32_t>(g_state.spotLights.size()),
    };

    float aspect = static_cast<float>(context.swapchain_extent.width) / static_cast<float>(context.swapchain_extent.height);
    if (g_state.projectionType == 0)
    {
        renderData.sceneUniform.proj = glm::perspectiveZO(g_state.camera.fovy, aspect, 0.1f, 100.0f);
    }
    else
    {
        renderData.sceneUniform.proj = glm::orthoZO(-aspect * 2.0f, aspect * 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);
    }
    renderData.sceneUniform.proj[1][1] *= -1; // Y-axis for Vulkan

    std::vector<graphics::core::RenderObjectData> renderLitObjects;
    renderLitObjects.reserve(g_state.litObjects.size());
    
    for (const auto& obj : g_state.litObjects)
    {
        renderLitObjects.push_back({
            .mesh  = obj.mesh,
            .modelMatrix = obj.transform.get_matrix(),
            .normalMatrix = obj.transform.get_normal_matrix(),
            .material = obj.material
        });
    }

    std::vector<graphics::core::RenderObjectData> renderUnlitObjects;
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
            .material = unlitMat
        });
    }

    std::vector<graphics::core::RenderObjectData> renderLineObjects;
    for (const auto& light : g_state.spotLights)
    {
        glm::vec3 pos = glm::vec3(light.position);
        glm::vec3 dir = glm::normalize(glm::vec3(light.direction));

        // Математический трюк: glm::lookAt создает матрицу View (World -> Camera).
        // Если взять её inverse(), мы получим матрицу Model (Camera -> World).
        // Так как наш конус изначально сгенерирован смотрящим в -Z (как и камера),
        // эта матрица идеально развернет конус в направлении вектора `dir`!
        glm::vec3 up = std::abs(dir.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::mat4 model = glm::inverse(glm::lookAt(pos, pos + dir, up));

        // Рассчитываем радиус конуса на основе угла свечения.
        // light.outerCutoff - это косинус угла, берем arccos, чтобы получить угол в радианах.
        float angle = std::acos(light.outerCutoff);
        float length = 3.0f; // Длина визуализируемого луча
        float radius = length * std::tan(angle); 
        
        // Масштабируем: X и Y - это радиус круга, Z - длина конуса
        model = glm::scale(model, glm::vec3(radius, radius, length));

        Material lineMat{};
        lineMat.albedo = light.color; // Конус будет цвета самого прожектора

        renderLineObjects.push_back({
            .mesh = &g_engine->coneMesh, // Форма конуса!
            .modelMatrix = model,
            .normalMatrix = glm::mat4(1.0f),
            .material = lineMat
        });
    }

    renderData.litObjects = renderLitObjects;
    renderData.unlitObjects = renderUnlitObjects;
    renderData.lineObjects = renderLineObjects;


    g_engine->draw(fd.command_buffer, fd.framebuffer, renderData);
}

} // namespace application