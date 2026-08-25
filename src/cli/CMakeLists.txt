add_library(lantalk_cli STATIC
    command_parser.cpp
    terminal_ui.cpp
)

target_include_directories(lantalk_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(lantalk_cli
    PUBLIC
        lantalk_chat
        lantalk_network
        lantalk_discovery
        lantalk_security
        lantalk_config
        lantalk_logging
)
