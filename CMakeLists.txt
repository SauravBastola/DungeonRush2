cmake_minimum_required(VERSION 3.16)
project(DungeonRush)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# --- SDL2 via pkg-config (cross-platform macOS / Linux) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)

# SDL2_image / _ttf / _mixer all re-declare -lSDL2, so the naive concatenation
# hands the linker the same library four times.  Dedupe before linking.
set(_ALL_SDL_LIBS
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
)
list(REMOVE_DUPLICATES _ALL_SDL_LIBS)

set(_ALL_SDL_INCLUDES
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)
list(REMOVE_DUPLICATES _ALL_SDL_INCLUDES)

set(_ALL_SDL_LIBDIRS
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_IMAGE_LIBRARY_DIRS}
    ${SDL2_TTF_LIBRARY_DIRS}
    ${SDL2_MIXER_LIBRARY_DIRS}
)
list(REMOVE_DUPLICATES _ALL_SDL_LIBDIRS)

# Fallback: also try find_package for systems without pkg-config
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

add_executable(${PROJECT_NAME}
        src/main.cpp
        src/AssetManager.cpp
        src/Depth.cpp
        src/Collision.cpp
        src/Game.cpp
        src/Map.cpp
        src/TextureManager.cpp
        src/Vector2D.cpp
        src/ECS/ECS.cpp
        src/Systems/Systems.cpp
        src/Systems/Parallax.cpp
        src/Systems/Atmosphere.cpp
        src/Systems/Scenery.cpp
        src/Systems/Vfx.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/ECS
        ${PROJECT_SOURCE_DIR}/src/Systems
        ${_ALL_SDL_INCLUDES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
        ${SDL2_CFLAGS_OTHER}
        ${SDL2_IMAGE_CFLAGS_OTHER}
        ${SDL2_TTF_CFLAGS_OTHER}
        ${SDL2_MIXER_CFLAGS_OTHER}
        -Wall -Wextra -Wpedantic
)

target_link_directories(${PROJECT_NAME} PRIVATE
        ${_ALL_SDL_LIBDIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${_ALL_SDL_LIBS}
)

# Define assets path for runtime (helps when binary is in bin/)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_PATH="${PROJECT_SOURCE_DIR}/assets")

# Keep legacy name Prelude as alias if needed
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:${PROJECT_NAME}> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Prelude
        COMMENT "Creating Prelude symlink"
)

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
