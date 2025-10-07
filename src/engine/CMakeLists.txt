add_library(rtype_engine
    rt/ecs/Registry.cpp
    src/Systems.cpp
)

target_include_directories(rtype_engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rtype_engine
    PUBLIC rtype_common
)
