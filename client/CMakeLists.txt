add_executable(r-type_client
        src/main.cpp
    src/ui/App.cpp
    # widgets
    src/ui/widgets/Button.cpp
    src/ui/widgets/InputBox.cpp
    src/ui/widgets/Title.cpp
    # screens
    src/ui/screens/Menu.cpp
    src/ui/screens/Singleplayer.cpp
    src/ui/screens/Multiplayer.cpp
    src/ui/screens/Waiting.cpp
    src/ui/screens/Options.cpp
    src/ui/screens/Leaderboard.cpp
    src/ui/screens/NotEnoughPlayers.cpp
    src/ui/screens/Gameplay.cpp
    # non-UI modules
    src/assets/Assets.cpp
    src/net/Net.cpp
    src/net/NetPackets.cpp
    src/utils/Utils.cpp
)

target_include_directories(r-type_client
    PRIVATE
        .
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/client/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(raylib REQUIRED)
find_package(asio REQUIRED)

target_link_libraries(r-type_client
    PRIVATE rtype_engine rtype_common raylib asio::asio
)

if (APPLE)
    # Raylib on macOS may need frameworks
    target_link_libraries(r-type_client PRIVATE
        "-framework IOKit" "-framework Cocoa" "-framework OpenGL"
    )
endif()
