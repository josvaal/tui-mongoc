cmake_minimum_required(VERSION 3.15)
project(mongodb-tui C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Find packages
# MongoDB C Driver
find_package(mongoc-1.0 REQUIRED)
find_package(bson-1.0 REQUIRED)

# Curses library (ncurses on Unix, pdcurses on Windows)
if(WIN32)
    # On Windows with vcpkg, use pdcurses (provided as unofficial-pdcurses)
    find_package(unofficial-pdcurses CONFIG REQUIRED)
    set(CURSES_LIBRARIES unofficial::pdcurses::pdcurses)
    set(CURSES_INCLUDE_DIRS "")
else()
    # On Unix systems, use ncurses
    find_package(Curses REQUIRED)
    set(CURSES_LIBRARIES ${CURSES_LIBRARIES})
    set(CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_DIRS})
endif()

# Source files
set(SOURCES
    src/main.c
    src/mongo_ops.c
    src/tui.c
    src/screens.c
    src/input.c
    src/json_display.c
    src/utils.c
)

# Header files (for IDE support)
set(HEADERS
    src/mongo_ops.h
    src/tui.h
    src/screens.h
    src/input.h
    src/json_display.h
    src/utils.h
)

# Create executable
add_executable(mongodb-tui ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(mongodb-tui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURSES_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(mongodb-tui PRIVATE
    mongo::mongoc_shared
    mongo::bson_shared
    ${CURSES_LIBRARIES}
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    if(MSVC)
        # Disable some MSVC warnings
        target_compile_definitions(mongodb-tui PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
else()
    # Unix-specific settings
    target_link_libraries(mongodb-tui PRIVATE m)
endif()

# Installation rules
install(TARGETS mongodb-tui DESTINATION bin)

# Print configuration summary
message(STATUS "")
message(STATUS "MongoDB TUI Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  MongoDB C Driver: Found")
message(STATUS "  Curses library: ${CURSES_LIBRARIES}")
message(STATUS "")
