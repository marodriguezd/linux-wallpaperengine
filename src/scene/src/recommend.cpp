#include "we/scene/recommend.hpp"

#include <algorithm>

namespace we::scene {

namespace {

// Scoring weights: each visible layer costs one unit, and each effect pass on
// it costs more because it adds a full-screen draw.
constexpr int effect_weight = 3;

struct Tier {
    int max_cost;
    int fps;
    const char* preset;
};

// Ordered from cheapest to most expensive. The first tier that fits wins.
constexpr Tier tiers[] = {
    {8, 60, "smooth"},
    {24, 30, "balanced"},
    {1'000'000, 15, "battery"},
};

} // namespace

Recommendation recommend (WallpaperType type, const Scene& scene) {
    Recommendation result;

    if (type == WallpaperType::video) {
        result.cost = 0;
        result.fps = 30;
        result.preset = "balanced";
        result.reasons.emplace_back ("video wallpapers decode at a fixed cost; 30 fps is smooth without extra load");
        return result;
    }

    int visible_layers = 0;
    int effect_passes = 0;
    for (const auto& image : scene.images) {
        if (!image.visible) {
            continue;
        }
        ++visible_layers;
        effect_passes += image.effect_count;
    }

    result.cost = visible_layers + effect_weight * effect_passes;
    result.reasons.push_back (std::to_string (visible_layers) + " visible layer(s)");
    if (effect_passes > 0) {
        result.reasons.push_back (std::to_string (effect_passes) + " effect pass(es) add full-screen draws");
    }

    for (const auto& tier : tiers) {
        if (result.cost <= tier.max_cost) {
            result.fps = tier.fps;
            result.preset = tier.preset;
            break;
        }
    }

    return result;
}

} // namespace we::scene
