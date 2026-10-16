#include "application.hpp"
#include "common.hpp"
#include "engine.hpp"
#include "graphics_internal.hpp"

#include <GLFW/glfw3.h>
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"
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

    glm::vec4 ambientLight{0.1f, 0.1f, 0.1f, 1.0f};
    glm::vec4 dirLightDir{0.0f, -1.0f, -1.0f, 0.0f};
    glm::vec4 dirLightColor{0.0f, 0.0f, 0.0f, 1.0f};

    std::vector<PointLight> pointLights{};
    std::vector<SpotLight> spotLights{};

    int projectionType{0}; // 0 - Perspective, 1 - Orthographic
    std::vector<AppObject> litObjects;
    std::vector<AppObject> unlitObjects;
    std::vector<AppObject> lineObjects;
} g_state;


namespace {

Material make_material(glm::vec4 albedo, glm::vec4 specular, float shininess) {
    return Material{
        .albedo        = albedo,
        .specularColor = specular,
        .shininess     = shininess,
    };
}

AppObject make_plane(
    std::string name,
    const graphics::core::Mesh* mesh,
    glm::vec3 position,
    glm::vec3 rotation,
    glm::vec3 scale,
    Material  material
) {
    return AppObject{
        .name      = std::move(name),
        .mesh      = mesh,
        .transform = { .position = position, .rotation = rotation, .scale = scale },
        .material  = material,
    };
}

} // namespace


bool initialize()
{
    g_engine = std::make_unique<graphics::core::VulkanEngine>();
    g_engine->init();

    const auto* plane = &g_engine->planeMesh;
    const auto* cube  = &g_engine->cubeMesh;

    g_state.camera.position = { 3.8f,  2.4f,  3.8f };
    g_state.camera.yaw      = -135.0f;
    g_state.camera.pitch    = -12.0f;
    g_state.camera.fovy     = glm::radians(50.0f);

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

    return true;
}


void shutdown()
{
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);
    g_engine->cleanup();
    g_engine.reset();
}

void update(double time)
{
    static double lastTime = 0.0;
    float dt = static_cast<float>(time - lastTime);
    lastTime = time;

    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard) // Camera motion (WASD + QE + Arrows | Shift - acceleration)
    {
        float cameraSpeed { g_state.camera.movement_speed * dt };
        if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) cameraSpeed *= 3.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(g_state.camera.yaw)) * cos(glm::radians(g_state.camera.pitch));
        front.y = sin(glm::radians(g_state.camera.pitch));
        front.z = sin(glm::radians(g_state.camera.yaw)) * cos(glm::radians(g_state.camera.pitch));
        front = glm::normalize(front);
        
        glm::vec3 right { glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) };
        glm::vec3 up { glm::normalize(glm::cross(right, front)) };

        if (ImGui::IsKeyDown(ImGuiKey_W)) g_state.camera.position += front * cameraSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) g_state.camera.position -= front * cameraSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) g_state.camera.position -= right * cameraSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) g_state.camera.position += right * cameraSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_E)) g_state.camera.position += up * cameraSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) g_state.camera.position -= up * cameraSpeed;
        
        float rotSpeed { g_state.camera.turn_speed * dt };
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))  g_state.camera.yaw   -= rotSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) g_state.camera.yaw   += rotSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow))    g_state.camera.pitch += rotSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow))  g_state.camera.pitch -= rotSpeed;

        if (g_state.camera.pitch > 89.0f)  g_state.camera.pitch = 89.0f;
        if (g_state.camera.pitch < -89.0f) g_state.camera.pitch = -89.0f;
    }

    ImGui::Begin("Lab 2: 3D Graphics");
    
    ImGui::Text("Press ARROWS to look around.");
    ImGui::Text("WASD - motion | QE - Up/Down | SHIFT - acceleration");
    ImGui::Separator();

	if (ImGui::CollapsingHeader("Camera"))
    {
		ImGui::TextUnformatted("WASD: move  |  Space/Ctrl: up/down  |  arrows: look");
		ImGui::SliderFloat("Movement speed", &g_state.camera.movement_speed, 0.5f, 12.0f);
		ImGui::SliderFloat("Turn speed", &g_state.camera.turn_speed, 20.0f, 180.0f);
		ImGui::SliderFloat("Field of view", &g_state.camera.fovy, 0.349055f, 2.0f);
	}

    if (ImGui::CollapsingHeader("Global Lighting"))
    {
        ImGui::ColorEdit3("Ambient Color", glm::value_ptr(g_state.ambientLight));
        
        ImGui::Text("Directional Light");
        ImGui::SliderFloat3("Direction", glm::value_ptr(g_state.dirLightDir), -1.0f, 1.0f);
        ImGui::ColorEdit3("Dir Color", glm::value_ptr(g_state.dirLightColor));
    }

    if (ImGui::CollapsingHeader("Point Lights"))
    {
        if (ImGui::Button("Add Point Light") && g_state.pointLights.size() < graphics::core::VulkanEngine::maxPointLights)
        {
            g_state.pointLights.push_back({
                .position = glm::vec4(0.0f, 2.0f, 0.0f, 1.0f),
                .color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f),
                .linear = 0.09f,
                .quadratic = 0.032f
            });
        }

        for (size_t i { 0 }; i < g_state.pointLights.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("Point Light %zu", i);
            ImGui::SliderFloat3("Position", glm::value_ptr(g_state.pointLights[i].position), -10.0f, 10.0f);
            ImGui::ColorEdit3("Color", glm::value_ptr(g_state.pointLights[i].color));
            ImGui::SliderFloat("Linear", &g_state.pointLights[i].linear, 0.0f, 1.0f);
            ImGui::SliderFloat("Quadratic", &g_state.pointLights[i].quadratic, 0.0f, 1.0f);
            
            if (ImGui::Button("Remove"))
            {
                g_state.pointLights.erase(g_state.pointLights.begin() + i);
                --i;
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Spot Lights"))
    {
        if (ImGui::Button("Add Spot Light") && g_state.spotLights.size() < graphics::core::VulkanEngine::maxSpotLights)
        {
            g_state.spotLights.push_back({
                .position = glm::vec4(0.0f, 5.0f, 0.0f, 1.0f),
                .direction = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
                .color = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f),
                .innerCutoff = glm::cos(glm::radians(12.5f)),
                .outerCutoff = glm::cos(glm::radians(17.5f)),
                .linear = 0.09f,
                .quadratic = 0.032f
            });
        }

        for (size_t i = 0; i < g_state.spotLights.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i + graphics::core::VulkanEngine::maxPointLights));
            ImGui::Text("Spot Light %zu", i);
            ImGui::SliderFloat3("Position", glm::value_ptr(g_state.spotLights[i].position), -10.0f, 10.0f);
            ImGui::SliderFloat3("Direction", glm::value_ptr(g_state.spotLights[i].direction), -1.0f, 1.0f);
            ImGui::ColorEdit3("Color", glm::value_ptr(g_state.spotLights[i].color));
            
            float innerAngle = glm::degrees(glm::acos(g_state.spotLights[i].innerCutoff));
            float outerAngle = glm::degrees(glm::acos(g_state.spotLights[i].outerCutoff));
            if (ImGui::SliderFloat("Inner Angle", &innerAngle, 0.0f, 45.0f)) g_state.spotLights[i].innerCutoff = glm::cos(glm::radians(innerAngle));
            if (ImGui::SliderFloat("Outer Angle", &outerAngle, 0.0f, 45.0f)) g_state.spotLights[i].outerCutoff = glm::cos(glm::radians(outerAngle));
            
            ImGui::SliderFloat("Linear", &g_state.spotLights[i].linear, 0.0f, 1.0f);
            ImGui::SliderFloat("Quadratic", &g_state.spotLights[i].quadratic, 0.0f, 1.0f);
            
            if (ImGui::Button("Remove"))
            {
                g_state.spotLights.erase(g_state.spotLights.begin() + i);
                --i;
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Objects"))
    {
        int ui_id { 0 };
        for (auto& obj : g_state.litObjects)
        {
            ImGui::PushID(ui_id++);
            if (ImGui::TreeNode(obj.name.c_str()))
            {
                ImGui::SliderFloat3("Position", glm::value_ptr(obj.transform.position), -5.0f, 5.0f);
                ImGui::SliderFloat3("Rotation", glm::value_ptr(obj.transform.rotation), 0.0f, 360.0f);
                ImGui::SliderFloat3("Scale", glm::value_ptr(obj.transform.scale), 0.1f, 5.0f);
                
                ImGui::ColorEdit3("Albedo (Color)", glm::value_ptr(obj.material.albedo));
                ImGui::ColorEdit3("Specular", glm::value_ptr(obj.material.specularColor));
                ImGui::SliderFloat("Shininess", &obj.material.shininess, 1.0f, 256.0f);

                if (ImGui::Button(obj.isAnimating ? "Pause Animation" : "Play Animation"))
                {
                    obj.isAnimating = !obj.isAnimating;
                }
                
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
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
            obj.transform.rotation.z += dt * 50.0f;
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
        .ambientLight = glm::vec4(g_state.ambientLight),
        .dirLightDir = glm::vec4(g_state.dirLightDir),
        .dirLightColor = glm::vec4(g_state.dirLightColor),
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
            .material = lineMat
        });

        renderLineObjects.push_back({
            .mesh = &g_engine->coneMesh,
            .modelMatrix = innerModel,
            .normalMatrix = glm::mat4(1.0f),
            .material = lineMat
        });
    }

    renderData.litObjects = renderLitObjects;
    renderData.unlitObjects = renderUnlitObjects;
    renderData.lineObjects = renderLineObjects;
    renderData.pointLights = g_state.pointLights;
    renderData.spotLights  = g_state.spotLights;


    g_engine->draw(fd.command_buffer, fd.framebuffer, renderData);
}

} // namespace application