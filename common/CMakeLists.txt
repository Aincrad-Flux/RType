add_library(rtype_common
        src/Protocol.cpp
)

target_include_directories(rtype_common
    PUBLIC include
    PRIVATE .
)
