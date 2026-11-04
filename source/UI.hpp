#pragma once

#include "common.hpp"
#include "engine.hpp"

namespace graphics::app::ui
{

void camera_motion(AppState& g_state, double dt);
void update_imgui(AppState& g_state, core::VulkanEngine& g_engine);

} // namespace graphics::app:ui