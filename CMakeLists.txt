cmake_minimum_required(VERSION 3.20)
project(Prelude)

set(CMAKE_CXX_STANDARD 14)

include_directories(
        C:/Dev/SDL2/include
        C:/Dev/SDL2_image/include
        C:/Dev/SDL2_ttf/include
)

link_directories(
        C:/Dev/SDL2/lib/x86
        C:/Dev/SDL2_image/lib/x86
        C:/Dev/SDL2_ttf/lib/x86
)

add_executable(
        Prelude
        main.cpp
        AssetManager.cpp
        AssetManager.h
        Collision.cpp
        Collision.h
        Game.cpp
        Game.h
        Map.cpp
        Map.h
        TextureManager.cpp
        TextureManager.h
        Vector2D.cpp
        Vector2D.h
        Animation.h
        ColliderComponent.h
        Components.h
        ECS.cpp
        ECS.h
        KeyboardController.h
        ProjectileComponent.h
        SpriteComponent.h
        TileComponent.h
        TransformComponent.h
        UILabel.h
)

target_link_libraries(Prelude SDL2main SDL2 SDL2_ttf SDL2_image)