add_executable(r-type_client
        src/main.cpp
        src/ui/App.cpp
        src/ui/Widgets.cpp
        src/ui/Screens.cpp
)

target_include_directories(r-type_client PRIVATE
        .
        include
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

add_custom_command(TARGET r-type_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:r-type_client>" "${CMAKE_SOURCE_DIR}/r-type_client"
    COMMENT "Copying r-type_client to project root"
)
