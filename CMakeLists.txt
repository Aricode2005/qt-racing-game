cmake_minimum_required(VERSION 3.16)
project(ApexVelocityRacing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Network Sql)

set(PROJECT_SOURCES
    main.cpp
    GameData.h
    PlayerProfile.h
    PlayerProfile.cpp
    GameWindow.h
    GameWindow.cpp
    GameEngine.h
    GameEngine.cpp
    Track.h
    Track.cpp
    RasterAssets.h
    RasterAssets.cpp
    RasterRenderer.h
    RasterRenderer.cpp
    AudioSynthesizer.h
    AudioSynthesizer.cpp
    NetworkManager.h
    NetworkManager.cpp
)

add_executable(ApexVelocity ${PROJECT_SOURCES})

target_link_libraries(ApexVelocity PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Network
    Qt6::Sql
)

if(MSVC)
    target_compile_options(ApexVelocity PRIVATE /O2 /fp:fast)
else()
    target_compile_options(ApexVelocity PRIVATE -O3 -ffast-math -funroll-loops)
endif()

add_executable(capture_screenshots
    capture_screenshots.cpp
    GameData.h
    PlayerProfile.h
    PlayerProfile.cpp
    GameWindow.h
    GameWindow.cpp
    GameEngine.h
    GameEngine.cpp
    Track.h
    Track.cpp
    RasterAssets.h
    RasterAssets.cpp
    RasterRenderer.h
    RasterRenderer.cpp
    AudioSynthesizer.h
    AudioSynthesizer.cpp
    NetworkManager.h
    NetworkManager.cpp
)

target_link_libraries(capture_screenshots PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Network
    Qt6::Sql
)

add_executable(network_auth_test
    network_auth_test.cpp
    GameData.h
    PlayerProfile.h
    PlayerProfile.cpp
    NetworkManager.h
    NetworkManager.cpp
    GameEngine.h
    GameEngine.cpp
    Track.h
    Track.cpp
    RasterAssets.h
    RasterAssets.cpp
    AudioSynthesizer.h
    AudioSynthesizer.cpp
)

target_link_libraries(network_auth_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Multimedia
    Qt6::Sql
)

add_executable(apex_relay_server
    apex_relay_server.cpp
)

target_link_libraries(apex_relay_server PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)
