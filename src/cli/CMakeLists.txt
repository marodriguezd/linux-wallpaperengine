add_executable(we_cli main.cpp)
we_configure_target(we_cli)

target_link_libraries(we_cli PRIVATE we_app we_scene we_display we_power we_presets we_steam we_gfx we_core)
set_target_properties(we_cli PROPERTIES OUTPUT_NAME we)
target_compile_definitions(we_cli PRIVATE WE_VERSION="${PROJECT_VERSION}")
