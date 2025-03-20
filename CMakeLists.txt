cmake_minimum_required(VERSION 3.10)
project(wayland-circle C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)

# Add compilation flags
add_compile_options(-Wall -Wextra -g -O2)

# Ensure protocols directory exists
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/protocols)

# Get full path to xdg-shell.xml
execute_process(
  COMMAND pkg-config --variable=pkgdatadir wayland-protocols
  OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DATADIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Commands to generate protocol files
execute_process(
  COMMAND wayland-scanner client-header
  ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/protocols/xdg-shell-client-protocol.h
  RESULT_VARIABLE SCANNER_RESULT
)
if(NOT ${SCANNER_RESULT} EQUAL 0)
  message(FATAL_ERROR "Failed to generate xdg-shell-client-protocol.h, error code: ${SCANNER_RESULT}")
endif()

execute_process(
  COMMAND wayland-scanner private-code
  ${WAYLAND_PROTOCOLS_DATADIR}/stable/xdg-shell/xdg-shell.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/protocols/xdg-shell-protocol.c
  RESULT_VARIABLE SCANNER_RESULT
)
if(NOT ${SCANNER_RESULT} EQUAL 0)
  message(FATAL_ERROR "Failed to generate xdg-shell-protocol.c, error code: ${SCANNER_RESULT}")
endif()

# Include directories
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/protocols
  ${WAYLAND_INCLUDE_DIRS}
)

# Add executable
add_executable(wayland-circle
  src/wayland-circle-xdg.c
  ${CMAKE_CURRENT_SOURCE_DIR}/protocols/xdg-shell-protocol.c
)

# Link libraries
target_link_libraries(wayland-circle
  ${WAYLAND_LIBRARIES}
  m
)

# Install target
install(TARGETS wayland-circle
  RUNTIME DESTINATION bin)