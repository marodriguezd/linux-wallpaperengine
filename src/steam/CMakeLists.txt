we_add_component(we_steam
    SOURCES
        src/package_archive.cpp
        src/steam_locator.cpp
        src/vdf.cpp
        src/vfs.cpp)

target_link_libraries(we_steam PUBLIC we_core)
