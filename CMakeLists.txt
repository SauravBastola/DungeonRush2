cmake_minimum_required(VERSION 3.20)
project(Prelude)

set(CMAKE_CXX_STANDARD 14)

include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/ECS
        C:/Dev/SDL2/include
        C:/Dev/SDL2_image/include
        C:/Dev/SDL2_ttf/include
        C:/Dev/SDL2_mixer/include
)

link_directories(
        C:/Dev/SDL2/lib/x86
        C:/Dev/SDL2_image/lib/x86
        C:/Dev/SDL2_ttf/lib/x86
        C:/Dev/SDL2_mixer/lib/x86
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
        src/ECS/RigidBodyComponent.h src/ECS/ScriptComponent.h)

target_link_libraries(Prelude SDL2main SDL2 SDL2_ttf SDL2_image SDL2_mixer)