add_executable(r-type_server
    src/main.cpp
    src/UdpServer.cpp
)

target_include_directories(r-type_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(asio REQUIRED)

target_link_libraries(r-type_server
    PRIVATE rtype_engine rtype_common asio::asio
)

if (WIN32)
    target_compile_definitions(r-type_server PRIVATE _WIN32_WINNT=0x0A00)
endif()

add_custom_command(TARGET r-type_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:r-type_server>" "${CMAKE_SOURCE_DIR}/r-type_server"
    COMMENT "Copying r-type_server to project root"
)
