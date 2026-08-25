add_library(lantalk_discovery STATIC
    lan_discovery.cpp
)

target_include_directories(lantalk_discovery PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(lantalk_discovery
    PUBLIC
        lantalk_logging
        lantalk_config
)

if(WIN32)
    target_link_libraries(lantalk_discovery PUBLIC ws2_32)
endif()
