find_package(nlohmann_json 3.11 REQUIRED)

we_add_component(we_desktophut
    SOURCES
        src/network.cpp
        src/client.cpp
        src/repository.cpp)

target_link_libraries(we_desktophut
    PUBLIC
        we_core
        we_steam
        we_scene
    PRIVATE
        nlohmann_json::nlohmann_json)
