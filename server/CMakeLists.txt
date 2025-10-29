add_executable(r-type_server
        src/main.cpp
        src/UdpServer.cpp
        src/TcpServer.cpp
        src/network/NetworkManager.cpp
        src/gameplay/GameSession.cpp
        src/instance/MatchInstance.cpp
)

target_include_directories(r-type_server PRIVATE include)

find_package(asio REQUIRED)

target_link_libraries(r-type_server
    PRIVATE rtype_engine rtype_common asio::asio
)

if (WIN32)
    target_compile_definitions(r-type_server PRIVATE _WIN32_WINNT=0x0A00)
endif()
