#include "support/harness.hpp"
#include "we/scene/recommend.hpp"

using we::scene::Recommendation;
using we::scene::Scene;
using we::scene::SceneImage;
using we::scene::WallpaperType;

namespace {

SceneImage layer (int effects, bool visible = true) {
	SceneImage image;
	image.visible = visible;
	image.effect_count = effects;
	return image;
}

Scene scene_with (std::vector<SceneImage> images) {
	Scene scene;
	scene.images = std::move (images);
	return scene;
}

} // namespace

TEST (recommend_video_is_moderate_and_fixed_cost) {
	const Recommendation rec = we::scene::recommend (WallpaperType::video, Scene {});
	CHECK_EQ (rec.fps, 30);
	CHECK_EQ (rec.preset, std::string ("balanced"));
	CHECK_EQ (rec.cost, 0);
}

TEST (recommend_light_scene_gets_smooth) {
	const Recommendation rec = we::scene::recommend (WallpaperType::scene, scene_with ({layer (0), layer (0)}));
	CHECK_EQ (rec.fps, 60);
	CHECK_EQ (rec.preset, std::string ("smooth"));
	CHECK_EQ (rec.cost, 2);
}

TEST (recommend_effects_raise_the_cost) {
	const Recommendation rec = we::scene::recommend (WallpaperType::scene, scene_with ({layer (2), layer (1)}));
	// 2 layers + 3 effect passes * 3 = 11, which is above the smooth tier.
	CHECK_EQ (rec.cost, 11);
	CHECK_EQ (rec.fps, 30);
	CHECK_EQ (rec.preset, std::string ("balanced"));
}

TEST (recommend_heavy_scene_gets_battery) {
	std::vector<SceneImage> images;
	for (int i = 0; i < 12; ++i) {
		images.push_back (layer (2));
	}
	const Recommendation rec = we::scene::recommend (WallpaperType::scene, scene_with (images));
	CHECK_EQ (rec.fps, 15);
	CHECK_EQ (rec.preset, std::string ("battery"));
}

TEST (recommend_ignores_hidden_layers) {
	const Recommendation rec = we::scene::recommend (
		WallpaperType::scene, scene_with ({layer (5, false), layer (0)}));
	CHECK_EQ (rec.cost, 1);
	CHECK_EQ (rec.fps, 60);
}
