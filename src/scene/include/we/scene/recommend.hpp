#pragma once

#include <string>
#include <vector>

#include "we/scene/project.hpp"
#include "we/scene/scene.hpp"

namespace we::scene {

// A local, explainable suggestion. It never talks to a server: the score is a
// simple count of what the renderer has to draw each frame.
struct Recommendation {
    int fps = 30;
    std::string preset = "balanced";
    int cost = 0;
    std::vector<std::string> reasons;
};

// Video wallpapers decode at a fixed cost, so they get a moderate rate. Scene
// wallpapers are scored by visible layers and effect passes, because each
// effect is an extra full-screen draw.
[[nodiscard]] Recommendation recommend (WallpaperType type, const Scene& scene);

} // namespace we::scene
