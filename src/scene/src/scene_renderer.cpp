#include "we/scene/scene_renderer.hpp"

#include "we/core/log.hpp"
#include "we/core/text.hpp"

#include <mpv/client.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace we::scene {

struct SceneAudio {
    mpv_handle* mpv = nullptr;
    std::vector<std::filesystem::path> temp_files;

    SceneAudio () = default;
    ~SceneAudio () {
	if (this->mpv != nullptr) {
	    mpv_terminate_destroy (this->mpv);
	    this->mpv = nullptr;
	}
	for (const auto& temp : this->temp_files) {
	    std::error_code ec;
	    std::filesystem::remove (temp, ec);
	}
    }

    SceneAudio (const SceneAudio&) = delete;
    SceneAudio& operator= (const SceneAudio&) = delete;
    SceneAudio (SceneAudio&& other) noexcept : mpv (other.mpv), temp_files (std::move (other.temp_files)) {
	other.mpv = nullptr;
    }
    SceneAudio& operator= (SceneAudio&& other) noexcept {
	if (this != &other) {
	    if (this->mpv != nullptr) {
		mpv_terminate_destroy (this->mpv);
	    }
	    for (const auto& temp : this->temp_files) {
		std::error_code ec;
		std::filesystem::remove (temp, ec);
	    }
	    this->mpv = other.mpv;
	    this->temp_files = std::move (other.temp_files);
	    other.mpv = nullptr;
	}
	return *this;
    }
};

SceneRenderer::SceneRenderer () = default;
SceneRenderer::~SceneRenderer () = default;
SceneRenderer::SceneRenderer (SceneRenderer&& other) noexcept = default;
SceneRenderer& SceneRenderer::operator= (SceneRenderer&& other) noexcept = default;
namespace {

    bool is_software_gl () {
	const auto* renderer = reinterpret_cast<const char*> (glGetString (GL_RENDERER));
	if (renderer == nullptr) {
	    return false;
	}
	std::string_view r { renderer };
	for (const std::string_view name : { "llvmpipe", "softpipe", "swrast", "SwiftShader" }) {
	    if (r.find (name) != std::string_view::npos) {
		return true;
	    }
	}
	return false;
    }

    core::Vec2 rotate (core::Vec2 value, float angle_radians) {
	const float cosine = std::cos (angle_radians);
	const float sine = std::sin (angle_radians);
	return { value.x * cosine - value.y * sine, value.x * sine + value.y * cosine };
    }

    // Wallpaper Engine scenes place objects in a centered space where y grows upward from the
    // bottom of the canvas. The renderer also draws in a centered y-up space.
    core::Vec2 scene_to_world (core::Vec2 point, int scene_width, int scene_height) {
	return { point.x - static_cast<float> (scene_width) * 0.5F,
		 point.y - static_cast<float> (scene_height) * 0.5F };
    }

    gfx::BlendMode parse_blend (std::string_view value) {
	if (value == "additive") {
	    return gfx::BlendMode::additive;
	}

	if (value == "translucent") {
	    return gfx::BlendMode::translucent;
	}

	return gfx::BlendMode::normal;
    }

    std::optional<std::string>
    load_shader_source (const AssetContainer& assets, const std::string& name, const char* ext) {
	std::string path = name;
	if (!path.ends_with (ext)) {
	    path += ext;
	}

	const std::vector<std::string> candidates
	    = { path, "shaders/" + path, "shaders/effects/" + path,
		path.starts_with ("effects/") ? ("shaders/" + path) : ("shaders/effects/" + path),
		path.starts_with ("shaders/") ? path : ("shaders/" + path) };

	for (const auto& candidate : candidates) {
	    auto res = assets.read_text (candidate);
	    if (res && !res->empty ()) {
		return *res;
	    }
	}

	return std::nullopt;
    }

    std::string resolve_includes (
	const AssetContainer& assets, std::string source, std::unordered_set<std::string>& included_files, int depth = 0
    ) {
	if (depth > 16) {
	    return source;
	}

	const std::string tag = "#include";
	std::size_t pos = 0;

	while ((pos = source.find (tag, pos)) != std::string::npos) {
	    const std::size_t q1 = source.find_first_of ("\"<", pos + tag.size ());
	    const std::size_t line_end = source.find ('\n', pos);

	    if (q1 == std::string::npos || (line_end != std::string::npos && q1 > line_end)) {
		pos += tag.size ();
		continue;
	    }

	    const char close_ch = source[q1] == '<' ? '>' : '"';
	    const std::size_t q2 = source.find (close_ch, q1 + 1);

	    if (q2 == std::string::npos || (line_end != std::string::npos && q2 > line_end)) {
		pos += tag.size ();
		continue;
	    }

	    const std::string inc_file = source.substr (q1 + 1, q2 - q1 - 1);
	    std::string replacement;

	    if (included_files.contains (inc_file)) {
		replacement = "\n";
	    } else {
		std::optional<std::string> inc_text;
		std::string resolved_path;

		const std::array<std::string, 3> candidates
		    = { inc_file, "shaders/" + inc_file, "shaders/effects/" + inc_file };

		for (const auto& candidate : candidates) {
		    if (included_files.contains (candidate)) {
			inc_text = "";
			resolved_path = candidate;
			break;
		    }
		    auto text = assets.read_text (candidate);
		    if (text && !text->empty ()) {
			inc_text = *text;
			resolved_path = candidate;
			break;
		    }
		}

		if (inc_text) {
		    included_files.insert (inc_file);
		    if (!resolved_path.empty ()) {
			included_files.insert (resolved_path);
		    }
		    if (!inc_text->empty ()) {
			replacement = resolve_includes (assets, *inc_text, included_files, depth + 1) + "\n";
		    } else {
			replacement = "\n";
		    }
		} else {
		    replacement = "// include not found: " + inc_file + "\n";
		}
	    }

	    const std::size_t replace_len
		= (line_end != std::string::npos) ? (line_end - pos + 1) : (source.size () - pos);
	    source.replace (pos, replace_len, replacement);
	    pos += replacement.size ();
	}

	return source;
    }

    std::string resolve_includes (const AssetContainer& assets, std::string source, int depth = 0) {
	std::unordered_set<std::string> included_files;
	return resolve_includes (assets, std::move (source), included_files, depth);
    }

    std::string strip_version_directive (std::string source) {
	std::size_t pos = source.find ("#version");
	while (pos != std::string::npos) {
	    const std::size_t line_end = source.find ('\n', pos);
	    if (line_end != std::string::npos) {
		source.erase (pos, line_end - pos + 1);
	    } else {
		source.erase (pos);
		break;
	    }
	    pos = source.find ("#version");
	}
	return source;
    }

    std::string replace_frag_color (std::string source) {
	const std::string from = "gl_FragColor";
	const std::string to = "out_FragColor";
	std::size_t pos = 0;

	while ((pos = source.find (from, pos)) != std::string::npos) {
	    source.replace (pos, from.size (), to);
	    pos += to.size ();
	}

	return source;
    }

    void parse_shader_metadata (
	const std::string& source, std::unordered_map<std::string, int>& combos,
	std::unordered_map<std::string, std::string>& uniform_mat_keys,
	std::unordered_map<std::string, std::string>& uniform_types,
	std::unordered_map<std::string, std::string>& uniform_defaults
    ) {
	std::istringstream stream (source);
	std::string line;

	while (std::getline (stream, line)) {
	    const std::size_t first = line.find_first_not_of (" \t\r");
	    if (first == std::string::npos) {
		continue;
	    }

	    const std::string_view trimmed { line.data () + first, line.size () - first };

	    if (trimmed.starts_with ("// [COMBO]")) {
		const std::size_t brace = trimmed.find ('{');
		if (brace != std::string_view::npos) {
		    auto cj = nlohmann::json::parse (trimmed.substr (brace), nullptr, false);
		    if (!cj.is_discarded () && cj.contains ("combo") && cj["combo"].is_string ()) {
			const std::string cname = cj["combo"].get<std::string> ();
			int def_val = 0;
			if (cj.contains ("default") && cj["default"].is_number ()) {
			    def_val = cj["default"].get<int> ();
			}
			if (!combos.contains (cname)) {
			    combos[cname] = def_val;
			}
		    }
		}
		continue;
	    }

	    if (trimmed.starts_with ("uniform ")) {
		const std::size_t semicolon = trimmed.find (';');
		const std::size_t comment_pos = trimmed.find ("//");

		if (semicolon != std::string_view::npos
		    && (comment_pos == std::string_view::npos || semicolon < comment_pos)) {
		    const std::string decl (trimmed.substr (8, semicolon - 8));
		    std::istringstream wstream (decl);
		    std::string type;
		    std::string name;

		    if (wstream >> type >> name) {
			const std::size_t bpos = name.find ('[');
			if (bpos != std::string::npos) {
			    name = name.substr (0, bpos);
			}

			uniform_types[name] = type;

			if (comment_pos != std::string_view::npos) {
			    const std::size_t brace = trimmed.find ('{', comment_pos);
			    if (brace != std::string_view::npos) {
				auto uj = nlohmann::json::parse (trimmed.substr (brace), nullptr, false);
				if (!uj.is_discarded ()) {
				    if (uj.contains ("material") && uj["material"].is_string ()) {
					uniform_mat_keys[name] = uj["material"].get<std::string> ();
				    }
				    if (uj.contains ("default")) {
					if (uj["default"].is_string ()) {
					    uniform_defaults[name] = uj["default"].get<std::string> ();
					} else if (uj["default"].is_number ()) {
					    uniform_defaults[name] = uj["default"].dump ();
					} else if (uj["default"].is_boolean ()) {
					    uniform_defaults[name] = uj["default"].get<bool> () ? "1" : "0";
					}
				    }
				    if (uj.contains ("combo") && uj["combo"].is_string ()) {
					const std::string cname = uj["combo"].get<std::string> ();
					if (!combos.contains (cname)) {
					    combos[cname] = 0;
					}
				    }
				}
			    }
			}
		    }
		}
	    }
	}
    }

    void apply_uniform_values (
	CompiledPass& pass, const std::unordered_map<std::string, std::string>& uniform_mat_keys,
	const std::unordered_map<std::string, std::string>& uniform_types,
	const std::unordered_map<std::string, std::string>& uniform_defaults,
	const std::unordered_map<std::string, std::string>& constant_values
    ) {
	for (const auto& [name, type] : uniform_types) {
	    std::string raw_val;

	    if (const auto mit = uniform_mat_keys.find (name); mit != uniform_mat_keys.end ()) {
		if (const auto cit = constant_values.find (mit->second); cit != constant_values.end ()) {
		    raw_val = cit->second;
		}
	    }

	    if (raw_val.empty ()) {
		if (const auto cit = constant_values.find (name); cit != constant_values.end ()) {
		    raw_val = cit->second;
		}
	    }

	    if (raw_val.empty ()) {
		if (const auto dit = uniform_defaults.find (name); dit != uniform_defaults.end ()) {
		    raw_val = dit->second;
		}
	    }

	    if (raw_val.empty ()) {
		continue;
	    }

	    if (type == "float") {
		if (const auto val = core::parse_float (raw_val)) {
		    pass.float_uniforms[name] = *val;
		}
	    } else if (type == "vec2") {
		std::istringstream ss (raw_val);
		float x = 0.0F;
		float y = 0.0F;
		if (ss >> x >> y) {
		    pass.vec2_uniforms[name] = core::Vec2 { x, y };
		}
	    } else if (type == "vec3") {
		std::istringstream ss (raw_val);
		float x = 0.0F;
		float y = 0.0F;
		float z = 0.0F;
		if (ss >> x >> y >> z) {
		    pass.vec3_uniforms[name] = core::Vec3 { x, y, z };
		}
	    } else if (type == "vec4") {
		std::istringstream ss (raw_val);
		float x = 0.0F;
		float y = 0.0F;
		float z = 0.0F;
		float w = 0.0F;
		if (ss >> x >> y >> z >> w) {
		    pass.vec4_uniforms[name] = core::Vec4 { x, y, z, w };
		}
	    } else if (type == "int") {
		if (const auto val = core::parse_int (raw_val)) {
		    pass.int_uniforms[name] = *val;
		}
	    }
	}
    }

    std::string build_shader_header (bool is_vertex, const std::unordered_map<std::string, int>& combos) {
	std::string header = "#version 330 core\n";
	header += "precision highp float;\n";
	header += "#define mul(x, y) ((y) * (x))\n";
	header += "#define max(x, y) max(y, x)\n";
	header += "#define lerp mix\n";
	header += "#define frac fract\n";
	header += "#define saturate(x) (clamp((x), 0.0, 1.0))\n";
	header += "#define CAST2(x) (vec2(x))\n";
	header += "#define CAST3(x) (vec3(x))\n";
	header += "#define CAST4(x) (vec4(x))\n";
	header += "#define CAST3X3(x) (mat3(x))\n";
	header += "#define float2 vec2\n";
	header += "#define float3 vec3\n";
	header += "#define float4 vec4\n";
	header += "#define int2 ivec2\n";
	header += "#define int3 ivec3\n";
	header += "#define int4 ivec4\n";
	header += "#define texSample2D texture\n";
	header += "#define texSample2DLod textureLod\n";
	header += "#define log10(x) (log2(x) * 0.301029995663981)\n";
	header += "#define atan2 atan\n";
	header += "#define fmod(x, y) ((x) - (y) * trunc((x) / (y)))\n";
	header += "#define ddx dFdx\n";
	header += "#define ddy(x) dFdy(-(x))\n";
	header += "#define GLSL 1\n";

	for (const auto& [k, v] : combos) {
	    header += "#define " + k + " " + std::to_string (v) + "\n";
	}

	if (is_vertex) {
	    header += "#define attribute in\n";
	    header += "#define varying out\n";
	} else {
	    header += "#define varying in\n";
	    header += "out vec4 out_FragColor;\n";
	    for (int i = 0; i < 8; ++i) {
		header += "uniform sampler2D g_Texture" + std::to_string (i) + ";\n";
	    }
	}

	return header;
    }

    std::string strip_sampler_declarations (std::string source) {
	for (int i = 0; i < 8; ++i) {
	    const std::string target = "uniform sampler2D g_Texture" + std::to_string (i);
	    std::size_t pos = 0;
	    while ((pos = source.find (target, pos)) != std::string::npos) {
		source.replace (pos, target.size (), "// " + target);
		pos += target.size () + 3;
	    }
	}
	return source;
    }

    std::optional<CompiledPass> compile_effect_pass (
	const AssetContainer& assets, const EffectPass& pass_def,
	const std::function<std::shared_ptr<gfx::Texture> (const std::string&, const char*)>& load_extra_texture,
	const std::unordered_map<std::string, TextureData>* cached_data = nullptr
    ) {
	if (pass_def.shader.empty ()) {
	    core::log_warn ("effect pass has no shader in material '", pass_def.material, "'");
	    return std::nullopt;
	}

	CompiledPass cpass;
	cpass.target = pass_def.target;
	cpass.binds = pass_def.binds;
	cpass.blend = parse_blend (pass_def.blending);
	cpass.compose = pass_def.compose;

	auto vert_raw = load_shader_source (assets, pass_def.shader, ".vert");
	auto frag_raw = load_shader_source (assets, pass_def.shader, ".frag");

	if (!vert_raw || !frag_raw) {
	    core::log_warn ("cannot find shaders for '", pass_def.shader, "'");
	    return std::nullopt;
	}

	std::unordered_map<std::string, int> combos = pass_def.combos;

	std::string vert_resolved = resolve_includes (assets, strip_version_directive (*vert_raw));
	std::string frag_resolved = resolve_includes (assets, strip_version_directive (*frag_raw));

	std::unordered_map<std::string, std::string> uniform_mat_keys;
	std::unordered_map<std::string, std::string> uniform_types;
	std::unordered_map<std::string, std::string> uniform_defaults;

	parse_shader_metadata (vert_resolved, combos, uniform_mat_keys, uniform_types, uniform_defaults);
	parse_shader_metadata (frag_resolved, combos, uniform_mat_keys, uniform_types, uniform_defaults);

	const std::size_t max_tex = std::max (pass_def.textures.size (), std::size_t { 4 });
	for (std::size_t i = 0; i < max_tex; ++i) {
	    std::string tpath;
	    if (i < pass_def.textures.size ()) {
		tpath = pass_def.textures[i];
	    }
	    if ((tpath.empty () || tpath.starts_with ("_rt_")) && i > 0) {
		const std::string var_name = "g_Texture" + std::to_string (i);
		if (const auto dit = uniform_defaults.find (var_name); dit != uniform_defaults.end ()) {
		    tpath = dit->second;
		}
	    }
	    if (tpath.empty () || tpath.starts_with ("_rt_")) {
		cpass.textures.push_back (nullptr);
		cpass.texture_resolutions.push_back (core::Vec4 { 0.0F, 0.0F, 0.0F, 0.0F });
	    } else {
		auto tex = load_extra_texture (tpath, "effect texture");
		cpass.textures.push_back (tex);
		core::Vec4 res { 0.0F, 0.0F, 0.0F, 0.0F };
		if (cached_data != nullptr) {
		    if (const auto it = cached_data->find (tpath); it != cached_data->end ()) {
			const float tw = static_cast<float> (
			    it->second.texture_width > 0 ? it->second.texture_width : it->second.width
			);
			const float th = static_cast<float> (
			    it->second.texture_height > 0 ? it->second.texture_height : it->second.height
			);
			const float w = static_cast<float> (it->second.width);
			const float h = static_cast<float> (it->second.height);
			res = core::Vec4 { tw, th, w, h };
		    }
		}
		if (res.x <= 0.0F && tex != nullptr) {
		    const float w = static_cast<float> (tex->width ());
		    const float h = static_cast<float> (tex->height ());
		    res = core::Vec4 { w, h, w, h };
		}
		cpass.texture_resolutions.push_back (res);
	    }
	}

	if (cpass.textures.size () > 1 && cpass.textures[1] != nullptr) {
	    combos["MASK"] = 1;
	} else if (!combos.contains ("MASK")) {
	    combos["MASK"] = 0;
	}

	if (!combos.contains ("VERTICAL")) {
	    combos["VERTICAL"] = 0;
	}

	if (!combos.contains ("DUALWAVES")) {
	    if (pass_def.constant_values.contains ("direction2") || pass_def.constant_values.contains ("speed2")
		|| pass_def.constant_values.contains ("scale2")) {
		combos["DUALWAVES"] = 1;
	    } else {
		combos["DUALWAVES"] = 0;
	    }
	}

	if (!combos.contains ("PERSPECTIVE")) {
	    if (pass_def.constant_values.contains ("point0") || pass_def.constant_values.contains ("perspective")) {
		combos["PERSPECTIVE"] = 1;
	    } else {
		combos["PERSPECTIVE"] = 0;
	    }
	}

	if (!combos.contains ("TIMEOFFSET")) {
	    combos["TIMEOFFSET"] = (cpass.textures.size () > 2 && cpass.textures[2] != nullptr) ? 1 : 0;
	}

	std::string t0_path;
	if (!pass_def.textures.empty () && !pass_def.textures[0].empty ()) {
	    t0_path = pass_def.textures[0];
	} else if (const auto dit = uniform_defaults.find ("g_Texture0"); dit != uniform_defaults.end ()) {
	    t0_path = dit->second;
	}

	if (!t0_path.empty () && !t0_path.starts_with ("_rt_")) {
	    std::optional<std::uint32_t> fmt;
	    if (cached_data != nullptr) {
		if (const auto it = cached_data->find (t0_path); it != cached_data->end ()) {
		    fmt = it->second.format;
		}
	    }
	    if (!fmt.has_value ()) {
		auto tdata = load_texture_data (assets, t0_path);
		if (tdata) {
		    fmt = tdata->format;
		}
	    }
	    if (fmt.has_value ()) {
		if (*fmt == 8) {
		    combos["TEX0FORMAT"] = 8;
		} else if (*fmt == 9) {
		    combos["TEX0FORMAT"] = 9;
		}
	    }
	}

	apply_uniform_values (cpass, uniform_mat_keys, uniform_types, uniform_defaults, pass_def.constant_values);

	const std::string full_vert = build_shader_header (true, combos) + vert_resolved;
	const std::string full_frag
	    = build_shader_header (false, combos) + replace_frag_color (strip_sampler_declarations (frag_resolved));

	const std::array<std::pair<std::string, GLuint>, 2> attribs = { {
	    { "a_Position", 0 },
	    { "a_TexCoord", 1 },
	} };

	auto shader = gfx::Shader::build (full_vert, full_frag, pass_def.shader, attribs);

	if (!shader) {
	    core::log_warn ("failed to compile effect shader '", pass_def.shader, "': ", shader.error ().message);
	    return std::nullopt;
	}

	cpass.shader = std::move (*shader);
	return cpass;
    }

    using json = nlohmann::json;

    float get_number (const json& object, const char* key, float fallback = 0.0F) {
	const auto it = object.find (key);
	if (it == object.end ()) {
	    return fallback;
	}
	if (it->is_number ()) {
	    return it->get<float> ();
	}
	if (it->is_object ()) {
	    const auto nested = it->find ("value");
	    if (nested != it->end () && nested->is_number ()) {
		return nested->get<float> ();
	    }
	}
	if (it->is_string ()) {
	    if (const auto val = core::parse_float (it->get<std::string_view> ())) {
		return *val;
	    }
	}
	return fallback;
    }

    std::string get_text (const json& object, const char* key, std::string fallback = {}) {
	const auto it = object.find (key);
	if (it == object.end ()) {
	    return fallback;
	}
	if (it->is_string ()) {
	    return it->get<std::string> ();
	}
	if (it->is_object ()) {
	    const auto nested = it->find ("value");
	    if (nested != it->end () && nested->is_string ()) {
		return nested->get<std::string> ();
	    }
	}
	return fallback;
    }

    core::Vec3 get_vec3 (const json& object, const char* key, core::Vec3 fallback = {}) {
	const auto it = object.find (key);
	if (it == object.end ()) {
	    return fallback;
	}
	if (it->is_string ()) {
	    std::istringstream stream (it->get<std::string> ());
	    float x = 0.0F;
	    float y = 0.0F;
	    float z = 0.0F;
	    if (stream >> x >> y >> z) {
		return core::Vec3 { x, y, z };
	    }
	    return fallback;
	}
	if (it->is_array () && it->size () >= 3) {
	    return core::Vec3 { (*it)[0].get<float> (), (*it)[1].get<float> (), (*it)[2].get<float> () };
	}
	if (it->is_number ()) {
	    const float val = it->get<float> ();
	    return core::Vec3 { val, val, val };
	}
	return fallback;
    }

    TextureData make_procedural_particle_texture (const std::string& name) {
	TextureData data;
	data.width = 64;
	data.height = 64;
	data.rgba.resize (64 * 64 * 4);

	const bool is_drop = name.find ("drop") != std::string::npos || name.find ("rain") != std::string::npos;

	for (int y = 0; y < 64; ++y) {
	    for (int x = 0; x < 64; ++x) {
		const float fx = (static_cast<float> (x) + 0.5F - 32.0F) / 32.0F;
		const float fy = (static_cast<float> (y) + 0.5F - 32.0F) / 32.0F;

		float alpha = 0.0F;
		if (is_drop) {
		    const float width_factor = std::clamp (0.6F - 0.4F * fy, 0.15F, 1.0F);
		    const float dx = fx / width_factor;
		    const float dist_sq = dx * dx + fy * fy;
		    if (dist_sq < 1.0F) {
			alpha = std::exp (-dist_sq * 3.0F);
		    }
		} else {
		    const float dist_sq = fx * fx + fy * fy;
		    if (dist_sq < 1.0F) {
			alpha = std::exp (-dist_sq * 3.5F);
		    }
		}

		const std::size_t idx = (static_cast<std::size_t> (y) * 64U + static_cast<std::size_t> (x)) * 4U;
		data.rgba[idx + 0] = 255;
		data.rgba[idx + 1] = 255;
		data.rgba[idx + 2] = 255;
		data.rgba[idx + 3] = static_cast<std::uint8_t> (std::clamp (alpha * 255.0F, 0.0F, 255.0F));
	    }
	}

	return data;
    }

    float random_range (std::mt19937& rng, float a, float b) {
	const float lo = std::min (a, b);
	const float hi = std::max (a, b);
	if (std::abs (hi - lo) < 1.0e-7F) {
	    return lo;
	}
	std::uniform_real_distribution<float> dist (lo, hi);
	return dist (rng);
    }

} // namespace

void ParticleSystemState::spawn_particle (ParticleInstance& p, const ParticleEmitterDef& emitter) {
    if (emitter.type == EmitterType::sphere_random) {
	const float angle = random_range (this->rng, 0.0F, 6.2831853F);
	const float min_r = std::abs (emitter.distance_min.x);
	const float max_r = std::max (std::abs (emitter.distance_max.x), min_r + 0.001F);
	const float radius = std::sqrt (random_range (this->rng, min_r * min_r, max_r * max_r));

	core::Vec3 offset { radius * std::cos (angle) * emitter.directions.x,
			    radius * std::sin (angle) * emitter.directions.y, 0.0F };

	if (emitter.sign.x == 1.0F) {
	    offset.x = std::abs (offset.x);
	} else if (emitter.sign.x == -1.0F) {
	    offset.x = -std::abs (offset.x);
	}
	if (emitter.sign.y == 1.0F) {
	    offset.y = std::abs (offset.y);
	} else if (emitter.sign.y == -1.0F) {
	    offset.y = -std::abs (offset.y);
	}

	p.position = { this->origin.x + offset.x * this->scale.x + emitter.origin.x,
		       this->origin.y + offset.y * this->scale.y + emitter.origin.y,
		       this->origin.z + offset.z * this->scale.z + emitter.origin.z };

	if (emitter.speed_max > 0.0F || emitter.speed_min != 0.0F) {
	    const float speed = random_range (this->rng, emitter.speed_min, emitter.speed_max);
	    p.velocity = { std::cos (angle) * speed, std::sin (angle) * speed, 0.0F };
	} else {
	    p.velocity = {};
	}
    } else {
	const float max_x = std::abs (emitter.distance_max.x);
	const float max_y = std::abs (emitter.distance_max.y);
	const float max_z = std::abs (emitter.distance_max.z);

	core::Vec3 offset { random_range (this->rng, -max_x, max_x) * emitter.directions.x,
			    random_range (this->rng, -max_y, max_y) * emitter.directions.y,
			    random_range (this->rng, -max_z, max_z) * emitter.directions.z };

	if (emitter.sign.x == 1.0F) {
	    offset.x = std::abs (offset.x);
	} else if (emitter.sign.x == -1.0F) {
	    offset.x = -std::abs (offset.x);
	}
	if (emitter.sign.y == 1.0F) {
	    offset.y = std::abs (offset.y);
	} else if (emitter.sign.y == -1.0F) {
	    offset.y = -std::abs (offset.y);
	}

	p.position = { this->origin.x + offset.x * this->scale.x + emitter.origin.x,
		       this->origin.y + offset.y * this->scale.y + emitter.origin.y,
		       this->origin.z + offset.z * this->scale.z + emitter.origin.z };
	p.velocity = {};
    }

    p.rotation = {};
    p.angular_velocity = {};
    p.color = { this->instance_override.colorn.x * this->instance_override.color.x,
		this->instance_override.colorn.y * this->instance_override.color.y,
		this->instance_override.colorn.z * this->instance_override.color.z };
    p.alpha = this->instance_override.alpha;
    p.size = 20.0F * this->instance_override.size;
    p.lifetime = 1.0F * this->instance_override.lifetime;
    p.age = 0.0F;
    p.alive = true;
    p.random_frame = -1;
    if (this->animation_mode == "randomframe" && !this->frames.empty ()) {
	std::uniform_int_distribution<int> dist (0, static_cast<int> (this->frames.size () - 1));
	p.random_frame = dist (this->rng);
    }
    p.oscillate_alpha = {};
    p.oscillate_position = {};

    for (const auto& init : this->initializers) {
	switch (init.type) {
	    case InitializerType::lifetime_random:
		p.lifetime = random_range (this->rng, init.min_val, init.max_val) * this->instance_override.lifetime;
		break;
	    case InitializerType::size_random:
		{
		    const float t = std::pow (random_range (this->rng, 0.0F, 1.0F), init.exponent);
		    p.size = (init.min_val + t * (init.max_val - init.min_val)) * this->instance_override.size;
		    break;
		}
	    case InitializerType::alpha_random:
		p.alpha = random_range (this->rng, init.min_val, init.max_val) * this->instance_override.alpha;
		break;
	    case InitializerType::color_random:
		{
		    float r = random_range (this->rng, init.min_vec.x, init.max_vec.x);
		    float g = random_range (this->rng, init.min_vec.y, init.max_vec.y);
		    float b = random_range (this->rng, init.min_vec.z, init.max_vec.z);
		    if (r > 1.0F || g > 1.0F || b > 1.0F) {
			r /= 255.0F;
			g /= 255.0F;
			b /= 255.0F;
		    }
		    p.color = { p.color.x * r, p.color.y * g, p.color.z * b };
		    break;
		}
	    case InitializerType::velocity_random:
		p.velocity.x
		    += random_range (this->rng, init.min_vec.x, init.max_vec.x) * this->instance_override.speed;
		p.velocity.y
		    += -random_range (this->rng, init.min_vec.y, init.max_vec.y) * this->instance_override.speed;
		p.velocity.z
		    += random_range (this->rng, init.min_vec.z, init.max_vec.z) * this->instance_override.speed;
		break;
	    case InitializerType::rotation_random:
		p.rotation.z = random_range (this->rng, init.min_vec.z, init.max_vec.z);
		break;
	    case InitializerType::angular_velocity_random:
		p.angular_velocity.z = random_range (this->rng, init.min_vec.z, init.max_vec.z);
		break;
	    case InitializerType::unknown:
		break;
	}
    }

    p.initial.color = p.color;
    p.initial.alpha = p.alpha;
    p.initial.size = p.size;
    p.initial.lifetime = p.lifetime;
}

void ParticleSystemState::update (float dt, float /*current_time*/) {
    if (dt <= 0.0F) {
	return;
    }

    for (std::size_t e_idx = 0; e_idx < this->emitters.size (); ++e_idx) {
	const auto& emitter = this->emitters[e_idx];
	auto& timer = this->emitter_timers[e_idx];
	auto& inst_done = this->emitter_instantaneous_done[e_idx];

	std::uint32_t to_emit = 0;
	if (emitter.instantaneous > 0 && !inst_done) {
	    to_emit += static_cast<std::uint32_t> (
		static_cast<float> (emitter.instantaneous) * this->instance_override.count
	    );
	    inst_done = true;
	}

	const float effective_rate = emitter.rate * this->instance_override.rate;
	if (effective_rate > 0.0F) {
	    timer += dt * effective_rate;
	    const auto rate_count = static_cast<std::uint32_t> (timer);
	    to_emit += rate_count;
	    timer -= static_cast<float> (rate_count);
	}

	to_emit = std::min (to_emit, 500U);

	for (std::uint32_t i = 0; i < to_emit; ++i) {
	    bool reused = false;
	    for (auto& p : this->particles) {
		if (!p.alive) {
		    this->spawn_particle (p, emitter);
		    reused = true;
		    break;
		}
	    }
	    if (!reused && this->particles.size () < this->max_count) {
		ParticleInstance p;
		this->spawn_particle (p, emitter);
		this->particles.push_back (p);
	    }
	}
    }

    for (auto& p : this->particles) {
	if (!p.alive) {
	    continue;
	}

	p.age += dt;
	if (p.age >= p.lifetime) {
	    p.alive = false;
	    continue;
	}

	p.rotation.z += p.angular_velocity.z * dt;

	for (const auto& op : this->operators) {
	    switch (op.type) {
		case OperatorType::movement:
		    p.position.x += p.velocity.x * dt;
		    p.position.y += p.velocity.y * dt;
		    p.position.z += p.velocity.z * dt;

		    p.velocity.x += op.gravity.x * dt * this->instance_override.speed;
		    p.velocity.y += -op.gravity.y * dt * this->instance_override.speed;
		    p.velocity.z += op.gravity.z * dt * this->instance_override.speed;

		    {
			const float drag_factor = std::max (0.0F, 1.0F - (op.drag * dt));
			p.velocity.x *= drag_factor;
			p.velocity.y *= drag_factor;
			p.velocity.z *= drag_factor;
		    }
		    break;
		case OperatorType::alpha_fade:
		    {
			const float life_frac = p.lifetime > 0.0F ? p.age / p.lifetime : 1.0F;
			float fade = 1.0F;
			if (op.fade_in_time > 0.0F && life_frac < op.fade_in_time) {
			    fade = life_frac / op.fade_in_time;
			} else if (op.fade_out_time < 1.0F && life_frac > op.fade_out_time) {
			    fade = (1.0F - life_frac) / (1.0F - op.fade_out_time);
			}
			fade = std::clamp (fade, 0.0F, 1.0F);
			p.alpha = p.initial.alpha * fade;
			p.oscillate_alpha.base = p.alpha;
		    }
		    break;
		case OperatorType::oscillate_alpha:
		    {
			if (!p.oscillate_alpha.initialized) {
			    p.oscillate_alpha.frequency = random_range (this->rng, op.frequency_min, op.frequency_max);
			    p.oscillate_alpha.phase = random_range (this->rng, op.phase_min, op.phase_max + 6.283185F);
			    p.oscillate_alpha.scale_min = op.scale_min;
			    p.oscillate_alpha.scale_max = op.scale_max;
			    p.oscillate_alpha.base = p.alpha;
			    p.oscillate_alpha.initialized = true;
			}
			const float cos_val
			    = (std::cos (p.oscillate_alpha.frequency * p.age + p.oscillate_alpha.phase) + 1.0F) * 0.5F;
			const float mult = p.oscillate_alpha.scale_min
			    + cos_val * (p.oscillate_alpha.scale_max - p.oscillate_alpha.scale_min);
			p.alpha = p.oscillate_alpha.base * mult;
		    }
		    break;
		case OperatorType::size_change:
		    {
			const float life_frac = p.lifetime > 0.0F ? p.age / p.lifetime : 1.0F;
			float t = 0.0F;
			if (life_frac <= op.start_time) {
			    t = 0.0F;
			} else if (life_frac >= op.end_time) {
			    t = 1.0F;
			} else {
			    t = (life_frac - op.start_time) / (op.end_time - op.start_time);
			}
			const float mult = op.start_val + t * (op.end_val - op.start_val);
			p.size = p.initial.size * mult;
		    }
		    break;
		case OperatorType::oscillate_position:
		    {
			if (!p.oscillate_position.initialized) {
			    const float f_min = op.frequency_min;
			    const float f_max = std::max (op.frequency_min, op.frequency_max);
			    p.oscillate_position.frequency = {
				random_range (this->rng, f_min, f_max),
				random_range (this->rng, f_min, f_max),
				random_range (this->rng, f_min, f_max),
			    };
			    const float s_min = op.scale_min;
			    const float s_max = std::max (op.scale_min, op.scale_max);
			    p.oscillate_position.scale = {
				random_range (this->rng, s_min, s_max),
				random_range (this->rng, s_min, s_max),
				random_range (this->rng, s_min, s_max),
			    };
			    constexpr float two_pi = 6.2831853F;
			    const float p_max = op.phase_max > op.phase_min ? op.phase_max : op.phase_min + two_pi;
			    p.oscillate_position.phase = {
				random_range (this->rng, op.phase_min, p_max),
				random_range (this->rng, op.phase_min, p_max),
				random_range (this->rng, op.phase_min, p_max),
			    };
			    p.oscillate_position.current_offset = {};
			    p.oscillate_position.initialized = true;
			}

			// Subtract previous offset to prevent cumulative drift
			p.position.x -= p.oscillate_position.current_offset.x;
			p.position.y -= p.oscillate_position.current_offset.y;
			p.position.z -= p.oscillate_position.current_offset.z;

			const float speed_override = this->instance_override.speed;
			const core::Vec3 new_offset {
			    std::cos (p.oscillate_position.frequency.x * p.age + p.oscillate_position.phase.x)
				* p.oscillate_position.scale.x * op.mask.x * speed_override,
			    std::cos (p.oscillate_position.frequency.y * p.age + p.oscillate_position.phase.y)
				* p.oscillate_position.scale.y * op.mask.y * speed_override,
			    std::cos (p.oscillate_position.frequency.z * p.age + p.oscillate_position.phase.z)
				* p.oscillate_position.scale.z * op.mask.z * speed_override,
			};

			p.oscillate_position.current_offset = new_offset;
			p.position.x += new_offset.x;
			p.position.y += new_offset.y;
			p.position.z += new_offset.z;
		    }
		    break;
		case OperatorType::unknown:
		    break;
	    }
	}
    }
}

void ParticleSystemState::prewarm (float seconds) {
    if (seconds <= 0.0F) {
	return;
    }
    constexpr float step = 0.05F;
    float t = 0.0F;
    const float limit = std::min (seconds, 15.0F);
    while (t < limit) {
	this->update (step, t);
	t += step;
    }
}

core::Rect ParticleSystemState::get_particle_uv (const ParticleInstance& p) const {
    if (this->frames.empty ()) {
	return { 0.0F, 0.0F, 1.0F, 1.0F };
    }

    const float tw = this->texture_width > 0 ? static_cast<float> (this->texture_width)
					     : (this->texture ? static_cast<float> (this->texture->width ()) : 1.0F);
    const float th = this->texture_height > 0 ? static_cast<float> (this->texture_height)
					      : (this->texture ? static_cast<float> (this->texture->height ()) : 1.0F);

    if (tw <= 0.0F || th <= 0.0F) {
	return { 0.0F, 0.0F, 1.0F, 1.0F };
    }

    if (this->frames.size () == 1) {
	const auto& frame = this->frames.front ();
	return {
	    frame.x / tw,
	    frame.y / th,
	    frame.width / tw,
	    frame.height / th,
	};
    }

    std::size_t frame_idx = 0;
    const auto frame_count = this->frames.size ();
    const float anim_speed = this->sequence_multiplier > 0.0F ? this->sequence_multiplier : 1.0F;

    if (this->animation_mode == "randomframe") {
	if (p.random_frame >= 0) {
	    frame_idx = static_cast<std::size_t> (p.random_frame) % frame_count;
	} else {
	    frame_idx = 0;
	}
    } else if (this->animation_mode == "once") {
	const float life_frac = p.lifetime > 0.0F ? std::clamp (p.age / p.lifetime, 0.0F, 1.0F) : 0.0F;
	frame_idx = static_cast<std::size_t> (life_frac * static_cast<float> (frame_count) * anim_speed);
	if (frame_idx >= frame_count) {
	    frame_idx = frame_count - 1;
	}
    } else {
	// "sequence" or looping
	if (this->duration > 0.0F) {
	    float time_in_cycle = std::fmod (p.age * anim_speed, this->duration);
	    if (time_in_cycle < 0.0F) {
		time_in_cycle += this->duration;
	    }
	    for (std::size_t f = 0; f < frame_count; ++f) {
		time_in_cycle -= this->frames[f].frametime;
		if (time_in_cycle <= 0.0F || f == frame_count - 1) {
		    frame_idx = f;
		    break;
		}
	    }
	} else {
	    const float life_frac = p.lifetime > 0.0F ? p.age / p.lifetime : 0.0F;
	    float cycle_pos = std::fmod (
		life_frac * static_cast<float> (frame_count) * anim_speed, static_cast<float> (frame_count)
	    );
	    if (cycle_pos < 0.0F) {
		cycle_pos += static_cast<float> (frame_count);
	    }
	    frame_idx = std::min (static_cast<std::size_t> (cycle_pos), frame_count - 1);
	}
    }

    const auto& frame = this->frames[frame_idx];
    return {
	frame.x / tw,
	frame.y / th,
	frame.width / tw,
	frame.height / th,
    };
}

std::expected<SceneRenderer, core::Error>
SceneRenderer::build (const Scene& scene, const AssetContainer& assets, const SceneRendererOptions& options) {
    SceneRenderer result;
    result.camera_ = scene.camera;
    result.scene_width_ = std::max (scene.camera.width, 1);
    result.scene_height_ = std::max (scene.camera.height, 1);
    result.clear_color_ = scene.clear_color;

    // Wallpaper Engine's orthographic path translates the projection by the
    // saved camera eye and its view matrix cancels that offset again
    // (Camera::setOrthogonalProjection + lookAt), so scene placement is defined
    // by the centred scene bounds alone.
    result.view_projection_ = core::Mat4::ortho (
	-static_cast<float> (result.scene_width_) * 0.5F, static_cast<float> (result.scene_width_) * 0.5F,
	-static_cast<float> (result.scene_height_) * 0.5F, static_cast<float> (result.scene_height_) * 0.5F, -1000.0F,
	1000.0F
    );
    result.ambient_ = scene.ambient;

    for (const auto& light : scene.lights) {
	gfx::PointLight converted;
	const auto position
	    = scene_to_world ({ light.origin.x, light.origin.y }, result.scene_width_, result.scene_height_);
	converted.position = core::Vec3 { position.x, position.y, light.origin.z };
	converted.color = core::Vec3 { light.color.r, light.color.g, light.color.b };
	converted.intensity = light.intensity;
	converted.radius = light.radius;
	converted.exponent = light.exponent;
	result.lights_.push_back (converted);
    }

    if (result.lights_.size () > static_cast<std::size_t> (gfx::max_point_lights)) {
	core::log_info (
	    "scene: only the first ", gfx::max_point_lights, " of ", scene.lights.size (), " point lights are rendered"
	);
	result.lights_.resize (static_cast<std::size_t> (gfx::max_point_lights));
    }

    const auto load_extra_texture
	= [&assets, &result] (const std::string& path, const char* role) -> std::shared_ptr<gfx::Texture> {
	if (path.empty ()) {
	    return nullptr;
	}

	if (const auto cached = result.textures_.find (path); cached != result.textures_.end ()) {
	    return cached->second;
	}

	auto data = load_texture_data (assets, path);

	if (!data) {
	    core::log_warn ("cannot load ", role, " '", path, "': ", data.error ().message);
	    return nullptr;
	}

	if (data->is_video) {
	    core::log_warn (role, " '", path, "' is a video texture");
	    return nullptr;
	}

	auto uploaded = upload_texture (*data);

	if (!uploaded) {
	    core::log_warn ("cannot upload ", role, " '", path, "': ", uploaded.error ().message);
	    return nullptr;
	}

	auto texture = std::make_shared<gfx::Texture> (std::move (*uploaded));
	result.textures_.emplace (path, texture);
	result.texture_data_.emplace (path, std::move (*data));
	return texture;
    };

    for (const auto& image : scene.images) {
	if (!image.visible || image.texture_path.empty ()) {
	    continue;
	}

	if (image.texture_path == "_rt_FullFrameBuffer") {
	    Layer layer;
	    layer.is_effect = true;
	    layer.fullscreen = true;
	    layer.order_index = image.order_index;

	    for (const auto& eff : image.effects) {
		for (const auto& fbo_def : eff.fbos) {
		    if (!result.fbos_.contains (fbo_def.name)) {
			const int scale = fbo_def.scale > 0 ? fbo_def.scale : 1;
			const int fw = std::max (1, result.scene_width_ / scale);
			const int fh = std::max (1, result.scene_height_ / scale);
			auto fbo_named = gfx::Framebuffer::create (fw, fh, gfx::FramebufferFormat::rgba8);

			if (fbo_named) {
			    result.fbos_.emplace (fbo_def.name, std::move (*fbo_named));
			} else {
			    core::log_warn ("cannot create FBO '", fbo_def.name, "': ", fbo_named.error ().message);
			}
		    }
		}

		EffectInstance inst;

		for (const auto& pass_def : eff.passes) {
		    auto compiled = compile_effect_pass (assets, pass_def, load_extra_texture, &result.texture_data_);
		    if (compiled) {
			inst.passes.push_back (std::move (*compiled));
		    }
		}

		if (!inst.passes.empty ()) {
		    layer.effects.push_back (std::move (inst));
		}
	    }

	    result.layers_.push_back (std::move (layer));
	    result.uses_offscreen_ = true;
	    continue;
	}

	std::shared_ptr<gfx::Texture> texture;
	const auto cached = result.textures_.find (image.texture_path);
	std::unique_ptr<VideoPlayer> video;

	if (cached != result.textures_.end ()) {
	    texture = cached->second;
	} else {
	    auto data = load_texture_data (assets, image.texture_path);

	    if (!data) {
		core::log_warn ("cannot load texture '", image.texture_path, "': ", data.error ().message);
		continue;
	    }

	    if (data->is_video) {
		VideoOptions video_options;
		video_options.hardware_decode = options.hardware_decode && !is_software_gl ();
		auto player = VideoPlayer::create (video_options);

		if (!player) {
		    core::log_warn ("cannot create video for '", image.texture_path, "': ", player.error ().message);
		    continue;
		}

		std::vector<std::byte> stream (data->video.size ());
		std::memcpy (stream.data (), data->video.data (), data->video.size ());
		auto loaded = (*player)->load (stream, image.texture_path);

		if (!loaded) {
		    core::log_warn ("cannot load video '", image.texture_path, "': ", loaded.error ().message);
		    continue;
		}

		(*player)->set_loop (true);
		(*player)->play ();
		video = std::move (*player);
		// Video layers have no pixel mipmap to upload; the first frame sets the size.
		data->width = image.size.x > 0.0F ? static_cast<int> (image.size.x) : 1;
		data->height = image.size.y > 0.0F ? static_cast<int> (image.size.y) : 1;
		data->texture_width = data->width;
		data->texture_height = data->height;
	    }

	    if (!data->is_video) {
		auto uploaded = upload_texture (*data);

		if (!uploaded) {
		    core::log_warn ("cannot upload texture '", image.texture_path, "': ", uploaded.error ().message);
		    continue;
		}

		texture = std::make_shared<gfx::Texture> (std::move (*uploaded));
	    }

	    result.textures_.emplace (image.texture_path, texture);
	    result.texture_data_.emplace (image.texture_path, std::move (*data));
	}

	const auto metadata = result.texture_data_.find (image.texture_path);

	if (metadata == result.texture_data_.end ()) {
	    continue;
	}

	std::shared_ptr<gfx::Texture> normal_texture;

	if (image.reflective) {
	    core::log_debug (
		"image '", image.name, "': reflective material, normal map '",
		image.normal_texture_path.empty () ? "<none>" : image.normal_texture_path, "'"
	    );
	}

	if (!image.normal_texture_path.empty ()) {
	    normal_texture = load_extra_texture (image.normal_texture_path, "normal map");
	}

	std::shared_ptr<gfx::Texture> pbr_mask_texture;

	if (!image.pbr_mask_texture_path.empty ()) {
	    pbr_mask_texture = load_extra_texture (image.pbr_mask_texture_path, "PBR mask");
	}

	float width = image.size.x;
	float height = image.size.y;

	if (width <= 0.0F || height <= 0.0F) {
	    width = static_cast<float> (metadata->second.width);
	    height = static_cast<float> (metadata->second.height);
	}

	if (width <= 0.0F || height <= 0.0F) {
	    continue;
	}

	if (image.fullscreen) {
	    width = static_cast<float> (result.scene_width_);
	    height = static_cast<float> (result.scene_height_);
	}

	const float scaled_width = width * image.scale.x;
	const float scaled_height = height * image.scale.y;
	core::Vec2 center { image.origin.x, image.origin.y };

	if (image.fullscreen) {
	    center
		= { static_cast<float> (result.scene_width_) * 0.5F, static_cast<float> (result.scene_height_) * 0.5F };
	} else {
	    if (image.alignment.find ("top") != std::string::npos) {
		center.y -= scaled_height * 0.5F;
	    } else if (image.alignment.find ("bottom") != std::string::npos) {
		center.y += scaled_height * 0.5F;
	    }

	    if (image.alignment.find ("left") != std::string::npos) {
		center.x += scaled_width * 0.5F;
	    } else if (image.alignment.find ("right") != std::string::npos) {
		center.x -= scaled_width * 0.5F;
	    }
	}

	center = scene_to_world (center, result.scene_width_, result.scene_height_);

	// Wallpaper Engine stores angles in radians.
	const float angle = image.angles.z;
	const float half_width = scaled_width * 0.5F;
	const float half_height = scaled_height * 0.5F;

	const std::array<core::Vec2, 4> local = {
	    core::Vec2 { -half_width, half_height },
	    core::Vec2 { half_width, half_height },
	    core::Vec2 { half_width, -half_height },
	    core::Vec2 { -half_width, -half_height },
	};

	Layer layer;
	layer.order_index = image.order_index;
	layer.texture = std::move (texture);
	layer.video = std::move (video);
	layer.blend = parse_blend (image.blending);
	layer.tint = core::Color { image.color.r, image.color.g, image.color.b, image.color.a * image.alpha };
	layer.z = image.origin.z;
	layer.lighting = image.lighting;
	layer.reflective = image.reflective && normal_texture != nullptr;
	layer.normal_texture = std::move (normal_texture);
	layer.pbr_mask_texture = std::move (pbr_mask_texture);
	layer.metallic_map = image.metallic_map;
	layer.roughness_map = image.roughness_map;
	layer.reflection_map = image.reflection_map;
	layer.emissive_map = image.emissive_map;
	layer.specular_tint = image.specular_tint;
	layer.emissive_color = image.emissive_color;
	layer.emissive_brightness = image.emissive_brightness;
	layer.reflectivity = image.reflectivity;
	layer.roughness = image.roughness;
	layer.metallic = image.metallic;
	layer.reflectivity_distance = image.reflectivity_distance;
	layer.tangent = rotate (core::Vec2 { 1.0F, 0.0F }, angle);
	layer.bitangent = rotate (core::Vec2 { 0.0F, 1.0F }, angle);

	for (std::size_t index = 0; index < local.size (); ++index) {
	    const auto rotated = rotate (local[index], angle);
	    layer.corners[index] = center + rotated;
	}

	layer.base_corners = layer.corners;
	layer.parallax_depth = image.parallax_depth;

	if (metadata != result.texture_data_.end () && !metadata->second.frames.empty ()) {
	    layer.frames = metadata->second.frames;
	    layer.duration = metadata->second.duration;
	    layer.texture_width
		= metadata->second.texture_width > 0 ? metadata->second.texture_width : metadata->second.width;
	    layer.texture_height
		= metadata->second.texture_height > 0 ? metadata->second.texture_height : metadata->second.height;

	    const auto& first_frame = layer.frames.front ();
	    layer.uv = core::Rect {
		first_frame.x / static_cast<float> (layer.texture_width),
		first_frame.y / static_cast<float> (layer.texture_height),
		first_frame.width / static_cast<float> (layer.texture_width),
		first_frame.height / static_cast<float> (layer.texture_height),
	    };
	} else {
	    // The TEX payload stores the cropped bitmap (the mipmap dimensions are
	    // its image width/height), while the header's full width/height only
	    // records the canvas it was cropped from. Sampling the whole uploaded
	    // texture is therefore what reproduces Wallpaper Engine's UVs.
	    layer.uv = core::Rect { 0.0F, 0.0F, 1.0F, 1.0F };
	}

	for (const auto& eff : image.effects) {
	    if (!eff.visible) {
		continue;
	    }

	    for (const auto& fbo_def : eff.fbos) {
		if (!result.fbos_.contains (fbo_def.name)) {
		    const int scale = fbo_def.scale > 0 ? fbo_def.scale : 1;
		    const int fw = std::max (1, static_cast<int> (width) / scale);
		    const int fh = std::max (1, static_cast<int> (height) / scale);
		    auto fbo_named = gfx::Framebuffer::create (fw, fh, gfx::FramebufferFormat::rgba8);

		    if (fbo_named) {
			result.fbos_.emplace (fbo_def.name, std::move (*fbo_named));
		    } else {
			core::log_warn ("cannot create FBO '", fbo_def.name, "': ", fbo_named.error ().message);
		    }
		}
	    }

	    EffectInstance inst;

	    for (const auto& pass_def : eff.passes) {
		auto compiled = compile_effect_pass (assets, pass_def, load_extra_texture, &result.texture_data_);
		if (compiled) {
		    inst.passes.push_back (std::move (*compiled));
		}
	    }

	    if (!inst.passes.empty ()) {
		core::log_info ("compiled ", inst.passes.size (), " passes for layer effect '", eff.file, "'");
		layer.effects.push_back (std::move (inst));
	    }
	}

	if (!layer.effects.empty ()) {
	    result.uses_offscreen_ = true;
	    const int lw = metadata != result.texture_data_.end () && metadata->second.width > 0
		? metadata->second.width
		: std::max (1, static_cast<int> (width));
	    const int lh = metadata != result.texture_data_.end () && metadata->second.height > 0
		? metadata->second.height
		: std::max (1, static_cast<int> (height));
	    auto fa = gfx::Framebuffer::create (lw, lh, gfx::FramebufferFormat::rgba8);
	    auto fb = gfx::Framebuffer::create (lw, lh, gfx::FramebufferFormat::rgba8);
	    if (fa && fb) {
		layer.fbo_a = std::move (*fa);
		layer.fbo_b = std::move (*fb);
	    } else {
		core::log_warn ("cannot create layer FBOs for '", image.name, "'");
	    }
	}

	if (image.puppet_mesh.has_value () && !image.puppet_mesh->positions.empty ()
	    && !image.puppet_mesh->indices.empty ()) {
	    Layer::PuppetGpu puppet;
	    puppet.index_count = static_cast<GLsizei> (image.puppet_mesh->indices.size ());
	    puppet.base_positions.reserve (image.puppet_mesh->positions.size ());
	    puppet.current_positions.reserve (image.puppet_mesh->positions.size ());

	    for (const auto& raw_pos : image.puppet_mesh->positions) {
		const float x_local = raw_pos.x * image.scale.x;
		const float y_local = raw_pos.y * image.scale.y;
		const auto rotated = rotate (core::Vec2 { x_local, y_local }, angle);
		const core::Vec3 world_pos { center.x + rotated.x, center.y + rotated.y, image.origin.z + raw_pos.z };
		puppet.base_positions.push_back (world_pos);
		puppet.current_positions.push_back (world_pos);
	    }

	    puppet.vao = gfx::GlVertexArray::generate ();
	    puppet.vbo_positions = gfx::GlBuffer::generate ();
	    puppet.vbo_uvs = gfx::GlBuffer::generate ();
	    puppet.ebo = gfx::GlBuffer::generate ();

	    puppet.vao.bind ();

	    puppet.vbo_positions.bind (GL_ARRAY_BUFFER);
	    glBufferData (
		GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (puppet.current_positions.size () * sizeof (core::Vec3)),
		puppet.current_positions.data (), GL_DYNAMIC_DRAW
	    );
	    glEnableVertexAttribArray (0);
	    glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, sizeof (core::Vec3), nullptr);

	    puppet.vbo_uvs.bind (GL_ARRAY_BUFFER);
	    glBufferData (
		GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (image.puppet_mesh->uvs.size () * sizeof (core::Vec2)),
		image.puppet_mesh->uvs.data (), GL_STATIC_DRAW
	    );
	    glEnableVertexAttribArray (1);
	    glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, sizeof (core::Vec2), nullptr);

	    puppet.ebo.bind (GL_ELEMENT_ARRAY_BUFFER);
	    glBufferData (
		GL_ELEMENT_ARRAY_BUFFER,
		static_cast<GLsizeiptr> (image.puppet_mesh->indices.size () * sizeof (std::uint16_t)),
		image.puppet_mesh->indices.data (), GL_STATIC_DRAW
	    );

	    gfx::GlVertexArray::unbind ();
	    gfx::GlBuffer::unbind (GL_ARRAY_BUFFER);

	    if (!layer.effects.empty ()) {
		std::vector<core::Vec3> local_positions;
		local_positions.reserve (image.puppet_mesh->positions.size ());
		for (const auto& raw_pos : image.puppet_mesh->positions) {
		    const float x_ndc = half_width > 0.0F ? (raw_pos.x * image.scale.x) / half_width : 0.0F;
		    const float y_ndc = half_height > 0.0F ? (raw_pos.y * image.scale.y) / half_height : 0.0F;
		    local_positions.push_back ({ x_ndc, y_ndc, 0.0F });
		}

		puppet.local_vao = gfx::GlVertexArray::generate ();
		puppet.local_vbo_positions = gfx::GlBuffer::generate ();

		puppet.local_vao.bind ();

		puppet.local_vbo_positions.bind (GL_ARRAY_BUFFER);
		glBufferData (
		    GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (local_positions.size () * sizeof (core::Vec3)),
		    local_positions.data (), GL_STATIC_DRAW
		);
		glEnableVertexAttribArray (0);
		glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, sizeof (core::Vec3), nullptr);

		puppet.vbo_uvs.bind (GL_ARRAY_BUFFER);
		glEnableVertexAttribArray (1);
		glVertexAttribPointer (1, 2, GL_FLOAT, GL_FALSE, sizeof (core::Vec2), nullptr);

		puppet.ebo.bind (GL_ELEMENT_ARRAY_BUFFER);

		gfx::GlVertexArray::unbind ();
		gfx::GlBuffer::unbind (GL_ARRAY_BUFFER);
	    }

	    layer.puppet_gpu = std::move (puppet);
	}

	result.layers_.push_back (std::move (layer));
    }

    const auto load_particle_file = [&assets, &load_extra_texture, &result] (
					const auto& self, const std::string& path, core::Vec3 origin, core::Vec3 scale,
					const ParticleInstanceOverride& override_inst, int order_index, int depth = 0
				    ) -> void {
	if (depth > 4 || path.empty ()) {
	    return;
	}

	auto source = assets.read_text (path);
	if (!source) {
	    core::log_warn ("cannot read particle file '", path, "': ", source.error ().message);
	    return;
	}

	json root = json::parse (*source, nullptr, false);
	if (root.is_discarded ()) {
	    core::log_warn ("cannot parse particle json '", path, "': malformed JSON");
	    return;
	}

	ParticleSystemState ps;
	ps.name = path;
	ps.origin = origin;
	ps.scale = scale;
	ps.instance_override = override_inst;
	ps.max_count = static_cast<std::uint32_t> (
	    std::clamp (get_number (root, "maxcount", 1000.0F) * override_inst.count, 1.0F, 25000.0F)
	);
	ps.start_time = get_number (root, "starttime", 0.0F);
	ps.animation_mode = get_text (root, "animationmode", "sequence");
	ps.sequence_multiplier = get_number (root, "sequencemultiplier", 1.0F);

	const std::string mat_path = get_text (root, "material");
	if (!mat_path.empty ()) {
	    if (auto mat_text = assets.read_text (mat_path); mat_text) {
		auto mj = json::parse (*mat_text, nullptr, false);
		if (!mj.is_discarded () && mj.contains ("passes") && mj["passes"].is_array ()
		    && !mj["passes"].empty ()) {
		    const auto& first = mj["passes"][0];
		    ps.blend = parse_blend (get_text (first, "blending", "translucent"));
		    if (first.contains ("combos") && first["combos"].is_object ()) {
			if (first["combos"].contains ("REFRACT") && first["combos"]["REFRACT"].is_number ()) {
			    ps.is_refract = first["combos"]["REFRACT"].get<int> () != 0;
			}
		    }

		    if (first.contains ("textures") && first["textures"].is_array () && !first["textures"].empty ()) {
			if (first["textures"][0].is_string ()) {
			    const std::string tpath = first["textures"][0].get<std::string> ();
			    if (tpath.find ("blank") != std::string::npos) {
				auto pdata = make_procedural_particle_texture ("drop");
				auto up = upload_texture (pdata);
				if (up) {
				    ps.texture = std::make_shared<gfx::Texture> (std::move (*up));
				}
			    } else {
				ps.texture = load_extra_texture (tpath, "particle texture");
				if (ps.texture == nullptr) {
				    auto pdata = make_procedural_particle_texture (tpath);
				    auto up = upload_texture (pdata);
				    if (up) {
					ps.texture = std::make_shared<gfx::Texture> (std::move (*up));
					result.textures_.emplace (tpath, ps.texture);
				    }
				}
				if (const auto meta_it = result.texture_data_.find (tpath);
				    meta_it != result.texture_data_.end ()) {
				    ps.frames = meta_it->second.frames;
				    ps.duration = meta_it->second.duration;
				    ps.texture_width = meta_it->second.texture_width > 0 ? meta_it->second.texture_width
											 : meta_it->second.width;
				    ps.texture_height = meta_it->second.texture_height > 0
					? meta_it->second.texture_height
					: meta_it->second.height;
				}
			    }
			}
		    }
		}
	    }
	}

	if (ps.texture == nullptr) {
	    auto pdata = make_procedural_particle_texture ("particle/halo");
	    auto up = upload_texture (pdata);
	    if (up) {
		ps.texture = std::make_shared<gfx::Texture> (std::move (*up));
	    }
	}

	if (root.contains ("renderer") && root["renderer"].is_array () && !root["renderer"].empty ()) {
	    const auto& rj = root["renderer"][0];
	    ps.renderer_def.name = get_text (rj, "name", "sprite");
	    ps.renderer_def.length = get_number (rj, "length", 1.0F);
	    ps.renderer_def.max_length = get_number (rj, "maxlength", 10.0F);
	    ps.renderer_def.min_length = get_number (rj, "minlength", 0.0F);
	}

	if (root.contains ("emitter") && root["emitter"].is_array ()) {
	    for (const auto& ej : root["emitter"]) {
		ParticleEmitterDef em;
		em.name = get_text (ej, "name", "boxrandom");
		if (em.name == "sphererandom") {
		    em.type = EmitterType::sphere_random;
		} else {
		    em.type = EmitterType::box_random;
		}
		em.origin = get_vec3 (ej, "origin", {});
		em.distance_min = get_vec3 (ej, "distancemin", {});
		em.distance_max = get_vec3 (ej, "distancemax", { 100.0F, 100.0F, 0.0F });
		em.directions = get_vec3 (ej, "directions", { 1.0F, 1.0F, 1.0F });
		em.sign = get_vec3 (ej, "sign", {});
		em.rate = get_number (ej, "rate", 0.0F);
		em.instantaneous = static_cast<int> (get_number (ej, "instantaneous", 0.0F));
		em.speed_min = get_number (ej, "speedmin", 0.0F);
		em.speed_max = get_number (ej, "speedmax", 0.0F);
		ps.emitters.push_back (em);
		ps.emitter_timers.push_back (0.0F);
		ps.emitter_instantaneous_done.push_back (false);
	    }
	}

	if (root.contains ("initializer") && root["initializer"].is_array ()) {
	    for (const auto& ij : root["initializer"]) {
		ParticleInitializerDef init;
		init.name = get_text (ij, "name");
		if (init.name == "lifetimerandom") {
		    init.type = InitializerType::lifetime_random;
		} else if (init.name == "sizerandom") {
		    init.type = InitializerType::size_random;
		} else if (init.name == "alpharandom") {
		    init.type = InitializerType::alpha_random;
		} else if (init.name == "colorrandom") {
		    init.type = InitializerType::color_random;
		} else if (init.name == "velocityrandom") {
		    init.type = InitializerType::velocity_random;
		} else if (init.name == "rotationrandom") {
		    init.type = InitializerType::rotation_random;
		} else if (init.name == "angularvelocityrandom") {
		    init.type = InitializerType::angular_velocity_random;
		}
		init.min_val = get_number (ij, "min", 0.0F);
		init.max_val = get_number (ij, "max", 1.0F);
		init.exponent = get_number (ij, "exponent", 1.0F);
		init.min_vec = get_vec3 (ij, "min", {});
		init.max_vec = get_vec3 (ij, "max", {});
		ps.initializers.push_back (init);
	    }
	}

	if (root.contains ("operator") && root["operator"].is_array ()) {
	    for (const auto& oj : root["operator"]) {
		ParticleOperatorDef op;
		op.name = get_text (oj, "name");
		if (op.name == "movement") {
		    op.type = OperatorType::movement;
		} else if (op.name == "alphafade") {
		    op.type = OperatorType::alpha_fade;
		} else if (op.name == "oscillatealpha") {
		    op.type = OperatorType::oscillate_alpha;
		} else if (op.name == "sizechange") {
		    op.type = OperatorType::size_change;
		} else if (op.name == "oscillateposition") {
		    op.type = OperatorType::oscillate_position;
		}
		op.gravity = get_vec3 (oj, "gravity", {});
		op.drag = get_number (oj, "drag", 0.0F);
		op.fade_in_time = get_number (oj, "fadeintime", 0.0F);
		op.fade_out_time = get_number (oj, "fadeouttime", 1.0F);
		op.frequency_min = get_number (oj, "frequencymin", 1.0F);
		op.frequency_max = get_number (oj, "frequencymax", 2.0F);
		op.scale_min = get_number (oj, "scalemin", 0.0F);
		op.scale_max = get_number (oj, "scalemax", 1.0F);
		op.phase_min = get_number (oj, "phasemin", 0.0F);
		op.phase_max = get_number (oj, "phasemax", 0.0F);
		op.start_time = get_number (oj, "starttime", 0.0F);
		op.end_time = get_number (oj, "endtime", 1.0F);
		op.start_val = get_number (oj, "startvalue", 0.0F);
		op.end_val = get_number (oj, "endvalue", 1.0F);
		op.mask = get_vec3 (oj, "mask", { 1.0F, 1.0F, 1.0F });
		ps.operators.push_back (op);
	    }
	}

	if (root.contains ("children") && root["children"].is_array ()) {
	    for (const auto& cj : root["children"]) {
		std::string cfile = get_text (cj, "name");
		if (cfile.empty ()) {
		    cfile = get_text (cj, "file");
		}
		if (cfile.empty () && cj.is_string ()) {
		    cfile = cj.get<std::string> ();
		}
		if (!cfile.empty ()) {
		    self (self, cfile, origin, scale, override_inst, order_index, depth + 1);
		}
	    }
	}

	ps.prewarm (ps.start_time);
	core::log_info (
	    "particle system '", ps.name, "': max_count=", ps.max_count, " blend=", static_cast<int> (ps.blend),
	    " texture=", ps.texture != nullptr ? 1 : 0
	);
	result.particle_systems_.push_back (std::move (ps));

	Layer player;
	player.is_particle = true;
	player.particle_system_index = result.particle_systems_.size () - 1;
	player.order_index = order_index;
	result.layers_.push_back (std::move (player));
    };

    for (const auto& particle : scene.particles) {
	if (!particle.visible) {
	    continue;
	}
	const auto pos
	    = scene_to_world ({ particle.origin.x, particle.origin.y }, result.scene_width_, result.scene_height_);
	const core::Vec3 world_pos { pos.x, pos.y, particle.origin.z };
	load_particle_file (
	    load_particle_file, particle.file, world_pos, particle.scale, particle.instance_override,
	    particle.order_index
	);
    }

    std::stable_sort (result.layers_.begin (), result.layers_.end (), [] (const Layer& a, const Layer& b) {
	return a.order_index < b.order_index;
    });

    if (scene.bloom.enabled) {
	result.bloom_enabled_ = true;
	result.bloom_ = scene.bloom;
	result.uses_offscreen_ = true;

	const int w4 = std::max (1, result.scene_width_ / 4);
	const int h4 = std::max (1, result.scene_height_ / 4);
	const int w8 = std::max (1, result.scene_width_ / 8);
	const int h8 = std::max (1, result.scene_height_ / 8);

	if (!result.fbos_.contains ("_rt_4FrameBuffer")) {
	    auto fbo4 = gfx::Framebuffer::create (w4, h4, gfx::FramebufferFormat::rgba8);
	    if (fbo4) {
		result.fbos_.emplace ("_rt_4FrameBuffer", std::move (*fbo4));
	    }
	}

	if (!result.fbos_.contains ("_rt_8FrameBuffer")) {
	    auto fbo8 = gfx::Framebuffer::create (w8, h8, gfx::FramebufferFormat::rgba8);
	    if (fbo8) {
		result.fbos_.emplace ("_rt_8FrameBuffer", std::move (*fbo8));
	    }
	}

	if (!result.fbos_.contains ("_rt_Bloom")) {
	    auto fbob = gfx::Framebuffer::create (w8, h8, gfx::FramebufferFormat::rgba8);
	    if (fbob) {
		result.fbos_.emplace ("_rt_Bloom", std::move (*fbob));
	    }
	}

	struct BloomPassDef {
	    const char* shader;
	    const char* target;
	    std::vector<EffectPassBind> binds;
	};

	const std::array<BloomPassDef, 4> bloom_defs = { {
	    { "downsample_quarter_bloom", "_rt_4FrameBuffer", { { "previous", 0 } } },
	    { "downsample_eighth_blur_v", "_rt_8FrameBuffer", { { "_rt_4FrameBuffer", 0 } } },
	    { "blur_h_bloom", "_rt_Bloom", { { "_rt_8FrameBuffer", 0 } } },
	    { "combine", "", { { "previous", 0 }, { "_rt_Bloom", 1 } } },
	} };

	for (const auto& bdef : bloom_defs) {
	    EffectPass ep;
	    ep.shader = bdef.shader;
	    ep.target = bdef.target;
	    ep.binds = bdef.binds;

	    auto compiled = compile_effect_pass (assets, ep, load_extra_texture, &result.texture_data_);

	    if (compiled) {
		if (std::string_view (bdef.shader) == "downsample_quarter_bloom") {
		    compiled->float_uniforms["g_BloomStrength"] = scene.bloom.strength;
		    compiled->float_uniforms["g_BloomThreshold"] = scene.bloom.threshold;
		    compiled->vec3_uniforms["g_BloomTint"] = scene.bloom.tint;
		}
		result.bloom_passes_.push_back (std::move (*compiled));
	    } else {
		core::log_warn ("failed to compile bloom pass '", bdef.shader, "', disabling bloom");
		result.bloom_enabled_ = false;
		result.bloom_passes_.clear ();
		break;
	    }
	}
    }

    if (result.uses_offscreen_) {
	auto fbo_a
	    = gfx::Framebuffer::create (result.scene_width_, result.scene_height_, gfx::FramebufferFormat::rgba8);
	if (!fbo_a) {
	    return core::fail (
		core::ErrorCode::unsupported, "failed to create offscreen scene FBO A: " + fbo_a.error ().message
	    );
	}
	result.fbo_scene_a_ = std::move (*fbo_a);

	auto fbo_b
	    = gfx::Framebuffer::create (result.scene_width_, result.scene_height_, gfx::FramebufferFormat::rgba8);
	if (!fbo_b) {
	    return core::fail (
		core::ErrorCode::unsupported, "failed to create offscreen scene FBO B: " + fbo_b.error ().message
	    );
	}
	result.fbo_scene_b_ = std::move (*fbo_b);

	struct QuadVertex {
	    float x;
	    float y;
	    float z;
	    float u;
	    float v;
	};

	const std::array<QuadVertex, 6> quad_verts = { {
	    { -1.0F, -1.0F, 0.0F, 0.0F, 0.0F },
	    { 1.0F, -1.0F, 0.0F, 1.0F, 0.0F },
	    { 1.0F, 1.0F, 0.0F, 1.0F, 1.0F },
	    { -1.0F, -1.0F, 0.0F, 0.0F, 0.0F },
	    { 1.0F, 1.0F, 0.0F, 1.0F, 1.0F },
	    { -1.0F, 1.0F, 0.0F, 0.0F, 1.0F },
	} };

	result.quad_vao_ = gfx::GlVertexArray::generate ();
	result.quad_vbo_ = gfx::GlBuffer::generate ();
	result.quad_vao_.bind ();
	result.quad_vbo_.bind (GL_ARRAY_BUFFER);
	glBufferData (GL_ARRAY_BUFFER, sizeof (quad_verts), quad_verts.data (), GL_STATIC_DRAW);
	glEnableVertexAttribArray (0);
	glVertexAttribPointer (
	    0, 3, GL_FLOAT, GL_FALSE, sizeof (QuadVertex), reinterpret_cast<const void*> (offsetof (QuadVertex, x))
	);
	glEnableVertexAttribArray (1);
	glVertexAttribPointer (
	    1, 2, GL_FLOAT, GL_FALSE, sizeof (QuadVertex), reinterpret_cast<const void*> (offsetof (QuadVertex, u))
	);
	gfx::GlVertexArray::unbind ();
	gfx::GlBuffer::unbind (GL_ARRAY_BUFFER);

	constexpr const char* blit_vert = "#version 330 core\n"
					  "layout (location = 0) in vec3 a_Position;\n"
					  "layout (location = 1) in vec2 a_TexCoord;\n"
					  "out vec2 v_TexCoord;\n"
					  "void main() {\n"
					  "    gl_Position = vec4(a_Position, 1.0);\n"
					  "    v_TexCoord = a_TexCoord;\n"
					  "}\n";

	constexpr const char* blit_frag = "#version 330 core\n"
					  "in vec2 v_TexCoord;\n"
					  "out vec4 out_FragColor;\n"
					  "uniform sampler2D u_Texture;\n"
					  "void main() {\n"
					  "    out_FragColor = texture(u_Texture, v_TexCoord);\n"
					  "}\n";

	auto blit_sh = gfx::Shader::build (blit_vert, blit_frag, "blit");
	if (!blit_sh) {
	    return core::fail (
		core::ErrorCode::unsupported, "failed to compile blit shader: " + blit_sh.error ().message
	    );
	}
	result.blit_shader_ = std::move (*blit_sh);
    }

    {
	constexpr const char* puppet_vert = "#version 330 core\n"
					    "layout (location = 0) in vec3 a_Position;\n"
					    "layout (location = 1) in vec2 a_TexCoord;\n"
					    "uniform mat4 u_Projection;\n"
					    "out vec2 v_TexCoord;\n"
					    "void main() {\n"
					    "    gl_Position = u_Projection * vec4(a_Position, 1.0);\n"
					    "    v_TexCoord = a_TexCoord;\n"
					    "}\n";

	constexpr const char* puppet_frag = "#version 330 core\n"
					    "in vec2 v_TexCoord;\n"
					    "out vec4 out_FragColor;\n"
					    "uniform sampler2D u_Texture;\n"
					    "uniform vec4 u_Tint;\n"
					    "void main() {\n"
					    "    out_FragColor = texture(u_Texture, v_TexCoord) * u_Tint;\n"
					    "}\n";

	auto puppet_sh = gfx::Shader::build (puppet_vert, puppet_frag, "puppet");
	if (!puppet_sh) {
	    return core::fail (
		core::ErrorCode::unsupported, "failed to compile puppet shader: " + puppet_sh.error ().message
	    );
	}
	result.puppet_shader_ = std::move (*puppet_sh);
    }

    if (options.audio_enabled && !scene.sounds.empty ()) {
	auto audio = std::make_unique<SceneAudio> ();
	audio->mpv = mpv_create ();
	if (audio->mpv != nullptr) {
	    mpv_set_option_string (audio->mpv, "vo", "null");
	    mpv_set_option_string (audio->mpv, "video", "no");
	    mpv_set_option_string (audio->mpv, "audio", "auto");
	    mpv_set_option_string (audio->mpv, "terminal", "no");
	    mpv_set_option_string (audio->mpv, "input-default-bindings", "no");
	    mpv_set_option_string (audio->mpv, "osc", "no");
	    mpv_set_option_string (audio->mpv, "loop-playlist", "inf");
	    const std::string volume_str
		= std::to_string (static_cast<int> (std::clamp (options.volume * 100.0F, 0.0F, 100.0F)));
	    mpv_set_option_string (audio->mpv, "volume", volume_str.c_str ());

	    if (mpv_initialize (audio->mpv) >= 0) {
		bool first = true;
		for (const auto& sound : scene.sounds) {
		    for (const auto& file : sound.files) {
			std::filesystem::path resolved_path;
			std::error_code ec;
			const auto direct = assets.root () / file;
			if (std::filesystem::is_regular_file (direct, ec)) {
			    resolved_path = std::filesystem::absolute (direct);
			} else if (auto data = assets.read (file); data && !data->empty ()) {
			    const auto temp = std::filesystem::temp_directory_path ()
				/ ("we-audio-" + std::to_string (reinterpret_cast<std::uintptr_t> (audio.get ())) + "-"
				   + std::to_string (audio->temp_files.size ()) + ".dat");
			    std::ofstream stream (temp, std::ios::binary | std::ios::trunc);
			    if (stream) {
				stream.write (
				    reinterpret_cast<const char*> (data->data ()),
				    static_cast<std::streamsize> (data->size ())
				);
				stream.close ();
				audio->temp_files.push_back (temp);
				resolved_path = temp;
			    }
			}

			if (!resolved_path.empty ()) {
			    const std::string path_str = resolved_path.string ();
			    const char* mode = first ? "replace" : "append";
			    const char* cmd[] = { "loadfile", path_str.c_str (), mode, nullptr };
			    mpv_command (audio->mpv, cmd);
			    first = false;
			}
		    }
		}
		if (!first) {
		    mpv_set_property_string (audio->mpv, "pause", "no");
		    result.audio_ = std::move (audio);
		}
	    }
	}
    }

    const auto image_layer_count = std::count_if (result.layers_.begin (), result.layers_.end (), [] (const Layer& l) {
	return !l.is_particle && !l.is_effect;
    });

    core::log_info (
	"scene: ", image_layer_count, " image layers, ", result.lights_.size (), " point lights, ",
	scene.skipped_objects, " objects not rendered"
    );
    return result;
}

bool SceneRenderer::update_videos () {
    bool changed = false;

    for (auto& layer : this->layers_) {
	if (layer.video != nullptr && layer.video->update ()) {
	    layer.texture = std::make_shared<gfx::Texture> (
		gfx::Texture::view (layer.video->texture ().id (), layer.video->width (), layer.video->height ())
	    );
	    changed = true;
	}
    }

    return changed;
}

void SceneRenderer::report_video_swap () {
    for (auto& layer : this->layers_) {
	if (layer.video != nullptr) {
	    layer.video->report_swap ();
	}
    }
}

void SceneRenderer::update_input (std::optional<core::Vec2> normalized_cursor, float dt) {
    if (!this->camera_.parallax_enabled) {
	this->parallax_displacement_ = { 0.0F, 0.0F };
	return;
    }

    core::Vec2 target { 0.0F, 0.0F };
    if (normalized_cursor.has_value ()) {
	const float cx = normalized_cursor->x - 0.5F;
	const float cy = 0.5F - normalized_cursor->y;
	target = core::Vec2 {
	    cx * this->camera_.parallax_mouse_influence,
	    cy * this->camera_.parallax_mouse_influence,
	};
    }

    float factor = 1.0F;
    if (this->camera_.parallax_delay > 0.0F && dt > 0.0F) {
	factor = std::clamp (1.0F - std::pow (this->camera_.parallax_delay, dt * 60.0F), 0.0F, 1.0F);
    }

    this->parallax_displacement_.x += (target.x - this->parallax_displacement_.x) * factor;
    this->parallax_displacement_.y += (target.y - this->parallax_displacement_.y) * factor;

    const float ref_size = static_cast<float> (this->scene_width_);

    for (auto& layer : this->layers_) {
	if (layer.is_effect || layer.is_particle) {
	    continue;
	}
	const float offset_x
	    = (layer.parallax_depth.x + this->camera_.parallax_amount) * this->parallax_displacement_.x * ref_size;
	const float offset_y
	    = (layer.parallax_depth.y + this->camera_.parallax_amount) * this->parallax_displacement_.y * ref_size;

	for (std::size_t i = 0; i < layer.corners.size (); ++i) {
	    layer.corners[i].x = layer.base_corners[i].x + offset_x;
	    layer.corners[i].y = layer.base_corners[i].y + offset_y;
	}

	if (layer.puppet_gpu.has_value ()) {
	    auto& pg = *layer.puppet_gpu;
	    for (std::size_t i = 0; i < pg.base_positions.size (); ++i) {
		pg.current_positions[i].x = pg.base_positions[i].x + offset_x;
		pg.current_positions[i].y = pg.base_positions[i].y + offset_y;
		pg.current_positions[i].z = pg.base_positions[i].z;
	    }
	    pg.vbo_positions.bind (GL_ARRAY_BUFFER);
	    glBufferSubData (
		GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr> (pg.current_positions.size () * sizeof (core::Vec3)),
		pg.current_positions.data ()
	    );
	    gfx::GlBuffer::unbind (GL_ARRAY_BUFFER);
	}
    }
}

void SceneRenderer::render_effect_pass (
    const CompiledPass& pass, gfx::Framebuffer*& current_composite, gfx::Framebuffer*& next_composite,
    float current_time
) const {
    if (!pass.shader.valid ()) {
	return;
    }

    gfx::Framebuffer* target_fbo = nullptr;
    int target_w = this->scene_width_;
    int target_h = this->scene_height_;

    if (pass.target.empty ()) {
	target_fbo = next_composite;
	if (target_fbo != nullptr) {
	    target_w = target_fbo->width ();
	    target_h = target_fbo->height ();
	} else {
	    target_w = this->scene_width_;
	    target_h = this->scene_height_;
	}
    } else {
	const auto it = this->fbos_.find (pass.target);
	if (it != this->fbos_.end ()) {
	    target_fbo = const_cast<gfx::Framebuffer*> (&it->second);
	    target_w = target_fbo->width ();
	    target_h = target_fbo->height ();
	} else {
	    core::log_warn ("target FBO '", pass.target, "' not found");
	    return;
	}
    }

    if (pass.target.empty () && current_composite != nullptr && next_composite != nullptr
	&& current_composite != next_composite
	&& (pass.blend == gfx::BlendMode::additive || pass.blend == gfx::BlendMode::translucent)) {
	glBindFramebuffer (GL_READ_FRAMEBUFFER, current_composite->id ());
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, next_composite->id ());
	glBlitFramebuffer (
	    0, 0, current_composite->width (), current_composite->height (), 0, 0, next_composite->width (),
	    next_composite->height (), GL_COLOR_BUFFER_BIT, GL_NEAREST
	);
    }

    target_fbo->bind ();
    glViewport (0, 0, target_w, target_h);

    glDisable (GL_DEPTH_TEST);

    if (pass.blend == gfx::BlendMode::additive) {
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);
    } else if (pass.blend == gfx::BlendMode::translucent) {
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
	glDisable (GL_BLEND);
    }

    pass.shader.use ();

    pass.shader.set_mat4 ("g_ModelViewProjectionMatrix", core::Mat4::identity ());
    pass.shader.set_vec2 (
	"g_TexelSize", core::Vec2 { 1.0F / static_cast<float> (target_w), 1.0F / static_cast<float> (target_h) }
    );
    pass.shader.set_float ("g_Time", current_time);

    struct BoundTexture {
	GLuint id = 0;
	int width = 0;
	int height = 0;
    };
    std::array<std::optional<BoundTexture>, 8> bound_textures;

    for (std::size_t index = 0; index < pass.textures.size () && index < 8; ++index) {
	if (pass.textures[index] && pass.textures[index]->valid ()) {
	    bound_textures[index] = BoundTexture {
		pass.textures[index]->id (),
		pass.textures[index]->width (),
		pass.textures[index]->height (),
	    };
	}
    }

    for (const auto& bind : pass.binds) {
	if (bind.index < 0 || bind.index >= 8) {
	    continue;
	}

	GLuint tex_id = 0;
	int tex_w = this->scene_width_;
	int tex_h = this->scene_height_;

	if (bind.name == "previous" || bind.name == "_rt_FullFrameBuffer") {
	    if (current_composite != nullptr) {
		tex_id = current_composite->color_texture ();
		tex_w = current_composite->width ();
		tex_h = current_composite->height ();
	    }
	} else if (const auto fit = this->fbos_.find (bind.name); fit != this->fbos_.end ()) {
	    tex_id = fit->second.color_texture ();
	    tex_w = fit->second.width ();
	    tex_h = fit->second.height ();
	}

	if (tex_id != 0) {
	    bound_textures[static_cast<std::size_t> (bind.index)] = BoundTexture { tex_id, tex_w, tex_h };
	}
    }

    if (!bound_textures[0].has_value () && current_composite != nullptr) {
	bound_textures[0] = BoundTexture {
	    current_composite->color_texture (),
	    current_composite->width (),
	    current_composite->height (),
	};
    }

    for (std::size_t index = 0; index < 8; ++index) {
	if (bound_textures[index].has_value ()) {
	    glActiveTexture (static_cast<GLenum> (GL_TEXTURE0 + index));
	    glBindTexture (GL_TEXTURE_2D, bound_textures[index]->id);
	    const std::string sidx = std::to_string (index);
	    pass.shader.set_int ("g_Texture" + sidx, static_cast<int> (index));

	    core::Vec4 res { static_cast<float> (bound_textures[index]->width),
			     static_cast<float> (bound_textures[index]->height),
			     static_cast<float> (bound_textures[index]->width),
			     static_cast<float> (bound_textures[index]->height) };
	    if (index < pass.texture_resolutions.size () && pass.texture_resolutions[index].x > 0.0F) {
		res = pass.texture_resolutions[index];
	    }
	    pass.shader.set_vec4 ("g_Texture" + sidx + "Resolution", res);
	}
    }

    for (const auto& [name, val] : pass.float_uniforms) {
	pass.shader.set_float (name, val);
    }
    for (const auto& [name, val] : pass.vec2_uniforms) {
	pass.shader.set_vec2 (name, val);
    }
    for (const auto& [name, val] : pass.vec3_uniforms) {
	pass.shader.set_vec3 (name, val);
    }
    for (const auto& [name, val] : pass.vec4_uniforms) {
	pass.shader.set_vec4 (name, val);
    }
    for (const auto& [name, val] : pass.int_uniforms) {
	pass.shader.set_int (name, val);
    }

    this->quad_vao_.bind ();
    glDrawArrays (GL_TRIANGLES, 0, 6);
    gfx::GlVertexArray::unbind ();

    if (pass.target.empty ()) {
	std::swap (current_composite, next_composite);
    }
}

void SceneRenderer::render_particles (
    gfx::Renderer& renderer, ParticleSystemState& ps, float dt, float current_time
) const {
    if (ps.texture == nullptr || !ps.texture->valid ()) {
	return;
    }

    ps.update (dt, current_time);

    const bool is_trail = ps.renderer_def.name == "spritetrail";

    for (const auto& p : ps.particles) {
	if (!p.alive || p.alpha <= 0.001F || p.size <= 0.01F) {
	    continue;
	}

	float alpha = std::clamp (p.alpha, 0.0F, 1.0F);
	const core::Color tint { p.color.x, p.color.y, p.color.z, alpha };
	const core::Rect uv = ps.get_particle_uv (p);

	std::array<core::Vec2, 4> corners;

	if (is_trail) {
	    const float spd = std::sqrt (p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y);
	    if (spd > 20.0F) {
		const float dir_x = p.velocity.x / spd;
		const float dir_y = p.velocity.y / spd;
		const float perp_x = -dir_y;
		const float perp_y = dir_x;

		const float trail_len = std::clamp (
		    spd * 0.05F * ps.renderer_def.length, p.size * 0.5F, p.size * ps.renderer_def.max_length
		);
		const float half_w = std::max (1.0F, p.size * 0.15F);
		const float half_l = trail_len * 0.5F;

		corners[0] = { p.position.x - dir_x * half_l - perp_x * half_w,
			       p.position.y - dir_y * half_l - perp_y * half_w };
		corners[1] = { p.position.x - dir_x * half_l + perp_x * half_w,
			       p.position.y - dir_y * half_l + perp_y * half_w };
		corners[2] = { p.position.x + dir_x * half_l + perp_x * half_w,
			       p.position.y + dir_y * half_l + perp_y * half_w };
		corners[3] = { p.position.x + dir_x * half_l - perp_x * half_w,
			       p.position.y - dir_y * half_l - perp_y * half_w };

		renderer.draw_quad (*ps.texture, corners, uv, tint, ps.blend, p.position.z);
		continue;
	    }
	}

	const float half_size = p.size * 0.5F;
	if (std::abs (p.rotation.z) > 1.0e-4F) {
	    const float cos_a = std::cos (-p.rotation.z);
	    const float sin_a = std::sin (-p.rotation.z);

	    const auto rot = [cos_a, sin_a, &p] (float dx, float dy) -> core::Vec2 {
		return { p.position.x + dx * cos_a - dy * sin_a, p.position.y + dx * sin_a + dy * cos_a };
	    };

	    corners[0] = rot (-half_size, half_size);
	    corners[1] = rot (half_size, half_size);
	    corners[2] = rot (half_size, -half_size);
	    corners[3] = rot (-half_size, -half_size);
	} else {
	    corners[0] = { p.position.x - half_size, p.position.y + half_size };
	    corners[1] = { p.position.x + half_size, p.position.y + half_size };
	    corners[2] = { p.position.x + half_size, p.position.y - half_size };
	    corners[3] = { p.position.x - half_size, p.position.y - half_size };
	}

	renderer.draw_quad (*ps.texture, corners, uv, tint, ps.blend, p.position.z);
    }
}

core::Rect SceneRenderer::layer_uv (std::size_t index, float elapsed) const {
    return this->calculate_layer_uv (this->layers_.at (index), elapsed);
}

core::Rect SceneRenderer::calculate_layer_uv (const Layer& layer, float elapsed) const {
    if (layer.frames.size () > 1 && layer.duration > 0.0F) {
	float time_in_cycle = std::fmod (elapsed, layer.duration);
	if (time_in_cycle < 0.0F) {
	    time_in_cycle += layer.duration;
	}

	for (const auto& frame : layer.frames) {
	    time_in_cycle -= frame.frametime;

	    if (time_in_cycle <= 0.0F) {
		return core::Rect {
		    frame.x / static_cast<float> (layer.texture_width),
		    frame.y / static_cast<float> (layer.texture_height),
		    frame.width / static_cast<float> (layer.texture_width),
		    frame.height / static_cast<float> (layer.texture_height),
		};
	    }
	}
    }

    return layer.uv;
}

void SceneRenderer::render (gfx::Renderer& renderer) const {
    const auto now = std::chrono::steady_clock::now ();
    float dt = std::chrono::duration<float> (now - this->last_render_time_).count ();
    if (dt <= 0.0F || dt > 0.1F) {
	dt = 1.0F / 60.0F;
    }
    this->last_render_time_ = now;
    const float elapsed = std::chrono::duration<float> (now - this->start_time_).count ();

    if (!this->uses_offscreen_) {
	renderer.set_projection (this->view_projection_);
	renderer.set_lighting (this->lights_, this->ambient_);

	bool frame_captured = false;

	for (const auto& layer : this->layers_) {
	    if (layer.is_particle) {
		if (layer.particle_system_index < this->particle_systems_.size ()) {
		    this->render_particles (
			renderer, this->particle_systems_[layer.particle_system_index], dt, elapsed
		    );
		    frame_captured = false;
		}
		continue;
	    }

	    if (layer.texture == nullptr || !layer.texture->valid ()) {
		continue;
	    }

	    const bool reflective
		= layer.reflective && layer.normal_texture != nullptr && layer.normal_texture->valid ();

	    const core::Rect current_uv = this->calculate_layer_uv (layer, elapsed);

	    if (layer.puppet_gpu.has_value ()) {
		renderer.flush ();
		this->draw_puppet_layer (layer, *layer.texture);
		frame_captured = false;
	    } else if (layer.lighting || reflective) {
		if (reflective) {
		    // One snapshot per run: later layers reuse the frame as it looked
		    // before the first reflective layer of the run.
		    if (!frame_captured) {
			renderer.capture_frame ();
			frame_captured = true;
		    }
		}

		gfx::LightingOptions lighting_options;
		lighting_options.normal_map = layer.normal_texture.get ();
		lighting_options.pbr_mask = layer.pbr_mask_texture.get ();
		lighting_options.roughness = layer.roughness;
		lighting_options.metallic = layer.metallic;
		lighting_options.specular_tint = layer.specular_tint;
		lighting_options.emissive_color = layer.emissive_color;
		lighting_options.emissive_brightness = layer.emissive_brightness;
		lighting_options.metallic_map = layer.metallic_map;
		lighting_options.roughness_map = layer.roughness_map;
		lighting_options.reflection_map = layer.reflection_map;
		lighting_options.emissive_map = layer.emissive_map;
		lighting_options.tangent = layer.tangent;
		lighting_options.bitangent = layer.bitangent;

		gfx::ReflectionOptions reflection_options;
		reflection_options.normal_map = layer.normal_texture.get ();
		reflection_options.pbr_mask = layer.pbr_mask_texture.get ();
		reflection_options.metallic_map = layer.metallic_map;
		reflection_options.roughness_map = layer.roughness_map;
		reflection_options.reflection_map = layer.reflection_map;
		reflection_options.reflectivity = layer.reflectivity;
		reflection_options.roughness = layer.roughness;
		reflection_options.metallic = layer.metallic;
		reflection_options.reflectivity_distance = layer.reflectivity_distance;
		reflection_options.tangent = layer.tangent;
		reflection_options.bitangent = layer.bitangent;

		renderer.draw_quad_material (
		    *layer.texture, layer.corners, current_uv, layer.tint, layer.blend, layer.z,
		    layer.lighting ? &lighting_options : nullptr, reflective ? &reflection_options : nullptr
		);

		if (!reflective) {
		    frame_captured = false;
		}
	    } else {
		renderer.draw_quad (*layer.texture, layer.corners, current_uv, layer.tint, layer.blend, layer.z);
		frame_captured = false;
	    }
	}
	return;
    }

    GLint dest_fbo = 0;
    glGetIntegerv (GL_DRAW_FRAMEBUFFER_BINDING, &dest_fbo);
    GLint dest_viewport[4] = {};
    glGetIntegerv (GL_VIEWPORT, dest_viewport);

    gfx::Framebuffer* current_composite = &this->fbo_scene_a_;
    gfx::Framebuffer* next_composite = &this->fbo_scene_b_;

    current_composite->bind ();
    glViewport (0, 0, this->scene_width_, this->scene_height_);
    glClearColor (this->clear_color_.r, this->clear_color_.g, this->clear_color_.b, this->clear_color_.a);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderer.set_projection (this->view_projection_);
    renderer.set_lighting (this->lights_, this->ambient_);
    bool frame_captured = false;

    for (const auto& layer : this->layers_) {
	if (layer.is_particle) {
	    if (layer.particle_system_index < this->particle_systems_.size ()) {
		this->render_particles (renderer, this->particle_systems_[layer.particle_system_index], dt, elapsed);
		frame_captured = false;
	    }
	    continue;
	}

	if (layer.is_effect) {
	    renderer.flush ();

	    for (const auto& eff : layer.effects) {
		for (const auto& pass : eff.passes) {
		    this->render_effect_pass (pass, current_composite, next_composite, elapsed);
		}
	    }

	    current_composite->bind ();
	    glViewport (0, 0, this->scene_width_, this->scene_height_);
	    frame_captured = false;
	    continue;
	}

	if (layer.texture == nullptr || !layer.texture->valid ()) {
	    continue;
	}

	const bool reflective = layer.reflective && layer.normal_texture != nullptr && layer.normal_texture->valid ();

	const core::Rect current_uv = this->calculate_layer_uv (layer, elapsed);

	const gfx::Texture* quad_texture = layer.texture.get ();

	if (!layer.effects.empty () && layer.fbo_a.has_value () && layer.fbo_b.has_value ()) {
	    renderer.flush ();

	    layer.fbo_a->bind ();
	    glViewport (0, 0, layer.fbo_a->width (), layer.fbo_a->height ());
	    glDisable (GL_DEPTH_TEST);
	    glDisable (GL_BLEND);

	    this->blit_shader_.use ();
	    this->blit_shader_.set_int ("u_Texture", 0);
	    glActiveTexture (GL_TEXTURE0);
	    glBindTexture (GL_TEXTURE_2D, layer.texture->id ());
	    if (layer.puppet_gpu.has_value () && layer.puppet_gpu->local_vao.valid ()) {
		glClearColor (0.0F, 0.0F, 0.0F, 0.0F);
		glClear (GL_COLOR_BUFFER_BIT);
		layer.puppet_gpu->local_vao.bind ();
		glDrawElements (GL_TRIANGLES, layer.puppet_gpu->index_count, GL_UNSIGNED_SHORT, nullptr);
		gfx::GlVertexArray::unbind ();
	    } else {
		this->quad_vao_.bind ();
		glDrawArrays (GL_TRIANGLES, 0, 6);
		gfx::GlVertexArray::unbind ();
	    }
	    glBindTexture (GL_TEXTURE_2D, 0);

	    gfx::Framebuffer* current_layer_fbo = &*layer.fbo_a;
	    gfx::Framebuffer* next_layer_fbo = &*layer.fbo_b;

	    for (const auto& eff : layer.effects) {
		for (const auto& pass : eff.passes) {
		    this->render_effect_pass (pass, current_layer_fbo, next_layer_fbo, elapsed);
		}
	    }

	    current_composite->bind ();
	    glViewport (0, 0, this->scene_width_, this->scene_height_);
	    renderer.set_projection (this->view_projection_);
	    renderer.set_lighting (this->lights_, this->ambient_);

	    layer.effect_view = current_layer_fbo->view ();
	    quad_texture = &layer.effect_view;
	}

	if (layer.puppet_gpu.has_value ()) {
	    renderer.flush ();
	    this->draw_puppet_layer (layer, *quad_texture);
	    frame_captured = false;
	} else if (layer.lighting || reflective) {
	    if (reflective) {
		if (!frame_captured) {
		    renderer.capture_frame ();
		    frame_captured = true;
		}
	    }

	    gfx::LightingOptions lighting_options;
	    lighting_options.normal_map = layer.normal_texture.get ();
	    lighting_options.pbr_mask = layer.pbr_mask_texture.get ();
	    lighting_options.roughness = layer.roughness;
	    lighting_options.metallic = layer.metallic;
	    lighting_options.specular_tint = layer.specular_tint;
	    lighting_options.emissive_color = layer.emissive_color;
	    lighting_options.emissive_brightness = layer.emissive_brightness;
	    lighting_options.metallic_map = layer.metallic_map;
	    lighting_options.roughness_map = layer.roughness_map;
	    lighting_options.reflection_map = layer.reflection_map;
	    lighting_options.emissive_map = layer.emissive_map;
	    lighting_options.tangent = layer.tangent;
	    lighting_options.bitangent = layer.bitangent;

	    gfx::ReflectionOptions reflection_options;
	    reflection_options.normal_map = layer.normal_texture.get ();
	    reflection_options.pbr_mask = layer.pbr_mask_texture.get ();
	    reflection_options.metallic_map = layer.metallic_map;
	    reflection_options.roughness_map = layer.roughness_map;
	    reflection_options.reflection_map = layer.reflection_map;
	    reflection_options.reflectivity = layer.reflectivity;
	    reflection_options.roughness = layer.roughness;
	    reflection_options.metallic = layer.metallic;
	    reflection_options.reflectivity_distance = layer.reflectivity_distance;
	    reflection_options.tangent = layer.tangent;
	    reflection_options.bitangent = layer.bitangent;

	    renderer.draw_quad_material (
		*quad_texture, layer.corners, current_uv, layer.tint, layer.blend, layer.z,
		layer.lighting ? &lighting_options : nullptr, reflective ? &reflection_options : nullptr
	    );

	    if (!reflective) {
		frame_captured = false;
	    }
	} else {
	    renderer.draw_quad (*quad_texture, layer.corners, current_uv, layer.tint, layer.blend, layer.z);
	    frame_captured = false;
	}

	if (!layer.effects.empty ()) {
	    renderer.flush ();
	}
    }

    renderer.flush ();

    if (this->bloom_enabled_) {
	for (const auto& pass : this->bloom_passes_) {
	    this->render_effect_pass (pass, current_composite, next_composite, elapsed);
	}
    }

    glBindFramebuffer (GL_FRAMEBUFFER, dest_fbo);
    glViewport (dest_viewport[0], dest_viewport[1], dest_viewport[2], dest_viewport[3]);
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_BLEND);

    this->blit_shader_.use ();
    this->blit_shader_.set_int ("u_Texture", 0);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, current_composite->color_texture ());

    this->quad_vao_.bind ();
    glDrawArrays (GL_TRIANGLES, 0, 6);
    gfx::GlVertexArray::unbind ();
    glBindTexture (GL_TEXTURE_2D, 0);
}

void SceneRenderer::draw_puppet_layer (const Layer& layer, const gfx::Texture& texture) const {
    if (!layer.puppet_gpu.has_value () || !layer.puppet_gpu->vao.valid ()) {
	return;
    }

    this->puppet_shader_.use ();
    this->puppet_shader_.set_mat4 ("u_Projection", this->view_projection_);
    this->puppet_shader_.set_int ("u_Texture", 0);
    this->puppet_shader_.set_vec4 ("u_Tint", core::Vec4 { layer.tint.r, layer.tint.g, layer.tint.b, layer.tint.a });

    glEnable (GL_BLEND);
    switch (layer.blend) {
	case gfx::BlendMode::normal:
	case gfx::BlendMode::translucent:
	    glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	    break;
	case gfx::BlendMode::additive:
	    glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
	    break;
    }

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texture.id ());

    layer.puppet_gpu->vao.bind ();
    glDrawElements (GL_TRIANGLES, layer.puppet_gpu->index_count, GL_UNSIGNED_SHORT, nullptr);
    gfx::GlVertexArray::unbind ();

    glBindTexture (GL_TEXTURE_2D, 0);
}

} // namespace we::scene
