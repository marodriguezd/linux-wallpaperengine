find_package(nlohmann_json 3.11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

we_add_component(we_scene
    SOURCES
        src/asset_container.cpp
        src/project.cpp
        src/recommend.cpp
        src/scene.cpp
        src/scene_renderer.cpp
        src/texture.cpp
        src/video.cpp)

target_link_libraries(we_scene
    PUBLIC
        we_core
        we_steam
        we_gfx
        nlohmann_json::nlohmann_json
        PkgConfig::MPV)

target_include_directories(we_scene
    SYSTEM PRIVATE
        "${PROJECT_SOURCE_DIR}/third_party/bcdec"
        "${PROJECT_SOURCE_DIR}/third_party/stb")
