add_library(ws2812)

target_sources(ws2812 PRIVATE
    ws2812.c
)

# Generate the PIO header for this library
pico_generate_pio_header(ws2812
    ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
)

target_link_libraries(ws2812 PUBLIC
    pico_stdlib
    hardware_pio
)

target_include_directories(ws2812 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)
