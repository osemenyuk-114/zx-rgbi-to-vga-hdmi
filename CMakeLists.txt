# Generated Cmake Pico project file
cmake_minimum_required(VERSION 3.13)

# Allow FetchContent_Populate for fine-grained dependency control
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXECUTABLE_NAME "ZX_RGBI_TO_VGA_HDMI")

# =============================================================================
# CONFIGURATION — edit these settings or pass via -D on the command line
# =============================================================================

# Board variant (pass -DBOARD=BOARD_XXX to override)
# Supported: BOARD_36LJU22, BOARD_38LJE24, BOARD_38LJU24, BOARD_RP2040_ZERO
#            BOARD_11XGA24_1, BOARD_11XGA24_2,
#            BOARD_LEO_V2, BOARD_LEO_V3, BOARD_LEO_V3_2040BT 
#            BOARD_09LJV23
#
if(NOT DEFINED BOARD)
    set(BOARD "BOARD_36LJU22")
    # set(BOARD "BOARD_38LJE24")
    # set(BOARD "BOARD_38LJU24")
    # set(BOARD "BOARD_RP2040_ZERO")
    # set(BOARD "BOARD_11XGA24_1")
    # set(BOARD "BOARD_11XGA24_2")
    # set(BOARD "BOARD_LEO_V2")
    # set(BOARD "BOARD_LEO_V3")
    # set(BOARD "BOARD_LEO_V3_2040BT")
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
if(BOARD STREQUAL "BOARD_RP2040_ZERO" OR BOARD STREQUAL "BOARD_LEO_V2" OR BOARD STREQUAL "BOARD_LEO_V3" OR BOARD STREQUAL "BOARD_LEO_V3_2040BT")
    set(USE_PICO2 OFF)
endif()

# Enable hardware configuration code for LEO v3 boards
if(BOARD STREQUAL "BOARD_LEO_V3" OR BOARD STREQUAL "BOARD_LEO_V3_2040BT")
    set(HW_CONFIG_ENABLE ON)
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

project(ZX_RGBI_TO_VGA_HDMI C CXX ASM)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# ====================================================================================

# Set board options based on board variant
if(BOARD STREQUAL "BOARD_36LJU22" OR BOARD STREQUAL "BOARD_RP2040_ZERO" OR BOARD STREQUAL "BOARD_09LJV23" OR BOARD STREQUAL "BOARD_LEO_V2")
    set(OSD_MENU_ENABLE ON)
    set(OSD_FF_ENABLE ON)
    set(PS2_KBD_ENABLE OFF)
    set(USB_KBD_ENABLE OFF)
    set(SERIAL_MENU_ENABLE ON)
elseif(BOARD STREQUAL "BOARD_38LJE24" OR BOARD STREQUAL "BOARD_LEO_V3" OR BOARD STREQUAL "BOARD_LEO_V3_2040BT")
    set(OSD_MENU_ENABLE ON)
    set(OSD_FF_ENABLE ON)
    set(PS2_KBD_ENABLE ON)
    set(USB_KBD_ENABLE ON)
    set(SERIAL_MENU_ENABLE OFF)
elseif(BOARD STREQUAL "BOARD_38LJU24")
    set(OSD_MENU_ENABLE ON)
    set(OSD_FF_ENABLE ON)
    set(PS2_KBD_ENABLE OFF)
    set(USB_KBD_ENABLE OFF)
    set(SERIAL_MENU_ENABLE ON)
elseif(BOARD STREQUAL "BOARD_11XGA24_1" OR BOARD STREQUAL "BOARD_11XGA24_2")
    set(OSD_MENU_ENABLE ON)
    set(OSD_FF_ENABLE OFF)
    set(PS2_KBD_ENABLE OFF)
    set(USB_KBD_ENABLE OFF)
    set(SERIAL_MENU_ENABLE ON)
else()
    message(FATAL_ERROR "Unsupported board variant: ${BOARD}")
endif()

# Automatically fetch pico_i2c_slave when FlashFloppy OSD is enabled.
# Only add the i2c_slave library subdirectory, skip examples.
if(OSD_FF_ENABLE)
    # Use modern FetchContent pattern while excluding example subdirectories
    FetchContent_Declare(
        pico_i2c_slave
        GIT_REPOSITORY https://github.com/vmilea/pico_i2c_slave.git
        GIT_TAG master
    )
    FetchContent_GetProperties(pico_i2c_slave)
    if(NOT pico_i2c_slave_POPULATED)
        FetchContent_Populate(pico_i2c_slave)
        # Only add the i2c_slave library, not the examples
        add_subdirectory(
            ${pico_i2c_slave_SOURCE_DIR}/i2c_slave
            ${pico_i2c_slave_BINARY_DIR}/i2c_slave
            EXCLUDE_FROM_ALL
        )
    endif()
endif()

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

# Conditionally add OSD common code (used by both MENU and FF)
if(OSD_MENU_ENABLE OR OSD_FF_ENABLE)
    target_sources(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/osd/osd.c
    )
endif()

# Conditionally add OSD menu source files
if(OSD_MENU_ENABLE)
    target_sources(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/osd/osd_menu.c
    )
endif()

# Conditionally add FlashFloppy OSD source files
if(OSD_FF_ENABLE)
    target_sources(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/osd/ff_osd.c
    )
endif()

# Conditionally add keyboard source files
if(PS2_KBD_ENABLE OR USB_KBD_ENABLE)
    target_sources(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/kbd/kbd.c
        ${CMAKE_CURRENT_LIST_DIR}/src/kbd/osd_kbd.c
        ${CMAKE_CURRENT_LIST_DIR}/src/kbd/zx_kbd.c
        ${CMAKE_CURRENT_LIST_DIR}/src/kbd/ch446q.c
        ${CMAKE_CURRENT_LIST_DIR}/src/kbd/epm3256.c
    )
endif()

if(PS2_KBD_ENABLE)
    target_sources(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/kbd/ps2_kbd.c
    )
endif()

if(USB_KBD_ENABLE)
    target_sources(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/kbd/usb_kbd.c
    )
endif()

if(HW_CONFIG_ENABLE)
    target_sources(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/hw_config.c
    )
endif()

# Generate PIO header
pico_generate_pio_header(${EXECUTABLE_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/video/video.pio
    ${CMAKE_CURRENT_LIST_DIR}/src/kbd/kbd.pio
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
    ${CMAKE_CURRENT_LIST_DIR}/src/kbd
    ${CMAKE_CURRENT_LIST_DIR}/src/usb
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

# Conditionally link TinyUSB host library for USB keyboard
if(USB_KBD_ENABLE)
    target_link_libraries(${EXECUTABLE_NAME}
        tinyusb_host
        tinyusb_board
    )
endif()

# Conditionally link I2C libraries for FlashFloppy OSD
if(OSD_FF_ENABLE)
    target_link_libraries(${EXECUTABLE_NAME}
        hardware_i2c
        pico_i2c_slave
    )
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

if(OSD_FF_ENABLE)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE OSD_FF_ENABLE)
endif()

if(SERIAL_MENU_ENABLE)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE SERIAL_MENU_ENABLE)
endif()

# Keyboard compile definitions
if(PS2_KBD_ENABLE)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE PS2_KBD_ENABLE)
endif()

if(USB_KBD_ENABLE)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
        USB_KBD_ENABLE
        NO_USB
        CFG_TUSB_CONFIG_FILE="tusb_config_host.h"
    )
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
