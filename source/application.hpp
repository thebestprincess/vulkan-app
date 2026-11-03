#pragma once

#include "graphics_internal.hpp"

namespace graphics::app
{

bool initialize();
void shutdown();

void update(double time);
void render(const internal::FrameData& fd);

} // namespace application