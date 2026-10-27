add_executable(we main.cpp)
we_configure_target(we)
add_executable(we_cli ALIAS we)

target_link_libraries(we PRIVATE we_app we_desktophut we_scene we_display we_power we_presets we_steam we_gfx we_core)
target_compile_definitions(we PRIVATE WE_VERSION="${PROJECT_VERSION}")

install(TARGETS we
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Application
)
