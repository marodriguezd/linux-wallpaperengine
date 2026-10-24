# Shared compiler configuration for every component of the engine.
#
# The `we_project_options` interface target carries the language standard,
# warning set, hardening flags and sanitizer selection. Components consume it
# through `we_configure_target()` (see WeTarget.cmake).

add_library(we_project_options INTERFACE)

target_compile_features(we_project_options INTERFACE cxx_std_23)

if (MSVC)
    target_compile_options(we_project_options INTERFACE /W4 /permissive-)
else ()
    target_compile_options(we_project_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wnull-dereference
        -Wundef)

    # Hardening. Fortify needs an optimising compiler, so it is only enabled
    # for optimised configurations (GCC warns otherwise).
    target_compile_options(we_project_options INTERFACE
        -fstack-protector-strong
        -fstack-clash-protection
        $<$<CONFIG:Release,RelWithDebInfo,MinSizeRel>:-D_FORTIFY_SOURCE=3>)

    target_link_options(we_project_options INTERFACE
        -Wl,-z,relro
        -Wl,-z,now)

    # Bounds-checked standard containers in debug builds. Cheap, and catches
    # off-by-one mistakes in cache/index code during tests.
    target_compile_definitions(we_project_options INTERFACE
        $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)
endif ()

if (WE_WARNINGS_AS_ERRORS)
    if (MSVC)
        target_compile_options(we_project_options INTERFACE /WX)
    else ()
        target_compile_options(we_project_options INTERFACE -Werror)
    endif ()
endif ()

set(WE_SANITIZER_FLAGS "")

if (WE_ENABLE_ASAN)
    list(APPEND WE_SANITIZER_FLAGS -fsanitize=address)
endif ()

if (WE_ENABLE_UBSAN)
    list(APPEND WE_SANITIZER_FLAGS -fsanitize=undefined)
endif ()

if (WE_ENABLE_TSAN)
    list(APPEND WE_SANITIZER_FLAGS -fsanitize=thread)
endif ()

if (WE_ENABLE_ASAN AND WE_ENABLE_TSAN)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer cannot be combined")
endif ()

if (WE_SANITIZER_FLAGS)
    target_compile_options(we_project_options INTERFACE
        ${WE_SANITIZER_FLAGS}
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all)
    target_link_options(we_project_options INTERFACE ${WE_SANITIZER_FLAGS})
endif ()
