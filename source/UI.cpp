#include "UI.hpp"
#include "common.hpp"
#include "engine.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{

void update_global_light(
    graphics::app::AppState& g_state
)
{
    if (ImGui::CollapsingHeader("Global Lighting"))
    {
        ImGui::ColorEdit3("Ambient Color", glm::value_ptr(g_state.ambientLight));
        
        ImGui::Text("Directional Light");
        ImGui::SliderFloat3("Direction", glm::value_ptr(g_state.dirLightDir), -1.0f, 1.0f);
        ImGui::ColorEdit3("Dir Color", glm::value_ptr(g_state.dirLightColor));
        if (ImGui::Button("Show spot lights")) { g_state.show_spot_lights = !g_state.show_spot_lights; }
        if (ImGui::Button("Show point lights")) { g_state.show_point_lights = !g_state.show_point_lights; }
    }
}

void update_camera(
    graphics::app::AppState& g_state
)
{
	if (ImGui::CollapsingHeader("Camera"))
    {
		ImGui::SliderFloat("Movement speed", &g_state.camera.movement_speed, 0.5f, 12.0f);
		ImGui::SliderFloat("Turn speed", &g_state.camera.turn_speed, 20.0f, 180.0f);
		ImGui::SliderFloat("Field of view", &g_state.camera.fovy, 0.349055f, 2.0f);
	}
}

void update_point_lights(
    graphics::app::AppState& g_state
)
{
    if (ImGui::CollapsingHeader("Point Lights"))
    {
        if (ImGui::Button("Add Point Light")
            && g_state.pointLights.size() < graphics::consts::maxPointLights)
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
}

void update_spot_lights(
    graphics::app::AppState& g_state
)
{
    if (ImGui::CollapsingHeader("Spot Lights"))
    {
        if (ImGui::Button("Add Spot Light") 
            && g_state.spotLights.size() < graphics::consts::maxSpotLights)
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
            ImGui::PushID(
                static_cast<int>(i + graphics::consts::maxPointLights)
            );
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
}

void update_objects(
    graphics::app::AppState& g_state,
    graphics::core::VulkanEngine& g_engine
)
{
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

                if (ImGui::BeginCombo("Texture", g_engine.loadedMaterials[obj.materialIndex].name.c_str()))
                {
                    for (size_t i = 0; i < g_engine.loadedMaterials.size(); ++i)
                    {
                        bool isSelected = (obj.materialIndex == i);
                        if (ImGui::Selectable(g_engine.loadedMaterials[i].name.c_str(), isSelected))
                        {
                            obj.materialIndex = i;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                
                ImGui::Checkbox("Complex Sampling Effect", &obj.useComplexSampling);

                if (ImGui::Button(obj.isAnimating ? "Pause Animation" : "Play Animation"))
                {
                    obj.isAnimating = !obj.isAnimating;
                }
                
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
}

} // namespace

namespace graphics::app::ui
{

void camera_motion(AppState& g_state, double dt)
{
    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard) // Camera motion (WASD + QE + Arrows | Shift - acceleration)
    {
        float cameraSpeed { static_cast<float>( g_state.camera.movement_speed * dt ) };
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
        
        float rotSpeed { static_cast<float>( g_state.camera.turn_speed * dt ) };
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))  g_state.camera.yaw   -= rotSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) g_state.camera.yaw   += rotSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow))    g_state.camera.pitch += rotSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow))  g_state.camera.pitch -= rotSpeed;

        if (g_state.camera.pitch > 89.0f)  g_state.camera.pitch = 89.0f;
        if (g_state.camera.pitch < -89.0f) g_state.camera.pitch = -89.0f;
    }
}

void update_imgui(AppState& g_state, core::VulkanEngine& g_engine)
{
    ImGui::Begin("Lab 2: 3D Graphics");
    
    ImGui::Text("Press ARROWS to look around.");
    ImGui::Text("WASD - motion | QE - Up/Down | SHIFT - acceleration");
    ImGui::Separator();

    update_camera(g_state);
    update_global_light(g_state);
    update_point_lights(g_state);
    update_spot_lights(g_state);
    update_objects(g_state, g_engine);
    
    ImGui::End();
}

} // namespace graphics::ui

