we_add_component(we_power
    SOURCES
        src/battery.cpp
        src/pacer.cpp)

target_link_libraries(we_power PUBLIC we_core)
