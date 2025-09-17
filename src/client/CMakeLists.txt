add_executable(r-type_client
    main.cpp
)

target_include_directories(r-type_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(raylib REQUIRED)

target_link_libraries(r-type_client
    PRIVATE rtype_engine rtype_common raylib::raylib
)

if (APPLE)
    # Raylib on macOS may need frameworks
    target_link_libraries(r-type_client PRIVATE
        "-framework IOKit" "-framework Cocoa" "-framework OpenGL"
    )
endif()
