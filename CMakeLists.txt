# Generated Cmake Pico project file
cmake_minimum_required(VERSION 3.13)

# Allow FetchContent_Populate for fine-grained dependency control
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXECUTABLE_NAME "RGB_TO_VGA_HDMI")

# =============================================================================
# CONFIGURATION — edit these settings or pass via -D on the command line
# =============================================================================

# Board variant (pass -DBOARD=BOARD_XXX to override)
# Supported: BOARD_36LJU22, BOARD_RP2040_ZERO
#            BOARD_11XGA24_1, BOARD_11XGA24_2,
#            BOARD_09LJV23
#
if(NOT DEFINED BOARD)
    set(BOARD "BOARD_36LJU22")
    # set(BOARD "BOARD_RP2040_ZERO")
    # set(BOARD "BOARD_11XGA24_1")
    # set(BOARD "BOARD_11XGA24_2")
    # set(BOARD "BOARD_09LJV23")
endif()

# Set to ON for Raspberry Pi Pico 2 (RP2350), OFF for original Pico (RP2040)
if(NOT DEFINED USE_PICO2)
    set(USE_PICO2 OFF)
endif()

# =============================================================================
# END OF CONFIGURATION
# =============================================================================

# Force USE_PICO2 OFF for RP2040-based boards (no RP2350 variant)
if(BOARD STREQUAL "BOARD_RP2040_ZERO")
    set(USE_PICO2 OFF)
endif()

# Select Pico board type based on board variant (FORCE overrides cached value)
if(USE_PICO2)
    set(PICO_BOARD pico2)
    set(PICO_PLATFORM rp2350-arm-s)
else()  
    set(PICO_BOARD pico)
    set(PICO_PLATFORM rp2040)
endif()

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(FetchContent)

project(RGB_TO_VGA_HDMI C CXX ASM)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# ====================================================================================

# Set board options
set(OSD_MENU_ENABLE ON)
set(SERIAL_MENU_ENABLE ON)

# Add executable
add_executable(${EXECUTABLE_NAME})

target_sources(
    ${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/g_config.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/src/settings.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/led.c
    ${CMAKE_CURRENT_LIST_DIR}/src/video/dvi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/video/rgb_capture.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/video/v_buf.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/video/vga.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/video/video_output.c
)

# Conditionally add serial menu (requires USB stdio)
if(SERIAL_MENU_ENABLE)
    target_sources(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/serial_menu.c
    )
endif()

# Conditionally add OSD menu source files
if(OSD_MENU_ENABLE)
    target_sources(
        ${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/osd/osd.c
        ${CMAKE_CURRENT_LIST_DIR}/src/osd/osd_menu.c
    )
endif()

# Generate PIO header
pico_generate_pio_header(${EXECUTABLE_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/video/video.pio
    ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${EXECUTABLE_NAME} 0)
if(USB_KBD_ENABLE)
    # USB port is in host mode — disable device-mode stdio entirely
    pico_enable_stdio_usb(${EXECUTABLE_NAME} 0)
else()
    pico_enable_stdio_usb(${EXECUTABLE_NAME} $<BOOL:${SERIAL_MENU_ENABLE}>)
endif()

# Add the standard include files to the build
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/video
    ${CMAKE_CURRENT_LIST_DIR}/src/osd
)

# Add any user requested libraries
target_link_libraries(${EXECUTABLE_NAME}
    hardware_pio
    hardware_pwm 
    hardware_dma 
    hardware_flash 
    pico_multicore 
    pico_unique_id  
    pico_stdlib
)

# Link USB stdio library when serial menu is active (needs USB device mode)
if(SERIAL_MENU_ENABLE)
    target_link_libraries(${EXECUTABLE_NAME} pico_stdio_usb)
endif()

target_compile_options(${EXECUTABLE_NAME} PRIVATE 
    -O3 
)

# Add compile definitions for board variant and Pico version
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ${BOARD})

# Add compile definitions for OSD features
if(OSD_MENU_ENABLE)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE OSD_MENU_ENABLE)
endif()

if(SERIAL_MENU_ENABLE)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE SERIAL_MENU_ENABLE)
endif()


# Add linker flag to print memory usage
target_link_options(${EXECUTABLE_NAME} PRIVATE
    -Wl,--print-memory-usage
)

# Output filename is fixed; CI renames per board/version during release
message(STATUS "Output firmware: ${EXECUTABLE_NAME}.uf2")

pico_add_extra_outputs(${EXECUTABLE_NAME})

# Extend the clean target to remove all object files (*.dir subdirectories only)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
    "${CMAKE_BINARY_DIR}/CMakeFiles/${EXECUTABLE_NAME}.dir"
)
