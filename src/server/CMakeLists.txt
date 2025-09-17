add_executable(r-type_server
    main.cpp
    UdpServer.cpp
)

target_include_directories(r-type_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(asio REQUIRED)

target_link_libraries(r-type_server
    PRIVATE rtype_engine rtype_common asio::asio
)

if (WIN32)
    target_compile_definitions(r-type_server PRIVATE _WIN32_WINNT=0x0A00)
endif()
