add_library(rtype_common
    Protocol.cpp
)

target_include_directories(rtype_common
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
