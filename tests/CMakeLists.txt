add_executable(we_tests
    main.cpp
    display_tests.cpp
    gfx_tests.cpp
    lz4_tests.cpp
    package_archive_tests.cpp
    presets_tests.cpp
    recommend_tests.cpp
    scene_tests.cpp
    steam_locator_tests.cpp
    vdf_tests.cpp
    vfs_tests.cpp)

we_configure_target(we_tests)
target_link_libraries(we_tests PRIVATE we_scene we_display we_steam we_gfx we_presets we_core)
target_include_directories(we_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(we_tests PRIVATE WE_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME we_tests COMMAND we_tests)

# Sanitizer runs keep leak detection on. Two third-party runtimes make that
# noisy on this stack: libmpv registers with the session D-Bus on first use, and
# the proprietary NVIDIA EGL ICD keeps per-process caches alive after
# eglTerminate(). The named libraries are listed in lsan.supp; for the ICD, whose
# module is dlopened and therefore unnamed in leak reports, sanitizer runs prefer
# Mesa's software vendor when it is installed so only our own allocations are
# judged.
if (WE_ENABLE_ASAN OR WE_ENABLE_UBSAN)
    set(WE_TEST_ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp")

    find_file(WE_MESA_EGL_VENDOR
        NAMES 50_mesa.json
        PATHS /usr/share/glvnd/egl_vendor.d /etc/glvnd/egl_vendor.d /usr/local/share/glvnd/egl_vendor.d)

    if (WE_MESA_EGL_VENDOR)
        list(APPEND WE_TEST_ENVIRONMENT
            "__EGL_VENDOR_LIBRARY_FILENAMES=${WE_MESA_EGL_VENDOR}")
    endif ()

    set_tests_properties(we_tests PROPERTIES ENVIRONMENT "${WE_TEST_ENVIRONMENT}")
endif ()
