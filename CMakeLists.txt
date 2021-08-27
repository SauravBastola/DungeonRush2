cmake_minimum_required(VERSION 3.16)

project(Prelude)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

SET(CMAKE_BUILD_TYPE Debug)

include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/ECS
        /usr/include/SDL2
)

link_directories(
)

add_executable(
        Prelude
        src/main.cpp
        src/AssetManager.cpp
        src/AssetManager.h
        src/Collision.cpp
        src/Collision.h
        src/Game.cpp
        src/Game.h
        src/Map.cpp
        src/Map.h
        src/TextureManager.cpp
        src/TextureManager.h
        src/Vector2D.cpp
        src/Vector2D.h
        src/ECS/Animation.h
        src/ECS/ColliderComponent.h
        src/ECS/Components.h
        src/ECS/ECS.cpp
        src/ECS/ECS.h
        src/ECS/ProjectileComponent.h
        src/ECS/SpriteComponent.h
        src/ECS/TileComponent.h
        src/ECS/TransformComponent.h
        src/ECS/UILabel.h
)

target_link_libraries(Prelude SDL2main SDL2 SDL2_ttf SDL2_image SDL2_mixer)
