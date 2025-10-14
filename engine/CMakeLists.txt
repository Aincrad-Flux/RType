add_library(rtype_engine
        rt/ecs/Registry.cpp
        src/Systems.cpp
)

target_include_directories(rtype_engine
    PUBLIC include
    PRIVATE .
)

target_link_libraries(rtype_engine
    PUBLIC rtype_common
)
