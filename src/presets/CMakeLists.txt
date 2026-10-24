find_package(nlohmann_json 3.11 REQUIRED)

we_add_component(we_presets
    SOURCES
        src/presets.cpp)

target_link_libraries(we_presets
    PUBLIC
        we_core
        nlohmann_json::nlohmann_json)
