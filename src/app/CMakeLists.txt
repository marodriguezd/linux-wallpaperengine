we_add_component(we_app
    SOURCES
        src/engine.cpp)

target_link_libraries(we_app
    PUBLIC
        we_core
        we_display
        we_gfx
        we_power
        we_scene
        we_steam)

target_include_directories(we_app
    SYSTEM PRIVATE
        "${PROJECT_SOURCE_DIR}/third_party/stb")
