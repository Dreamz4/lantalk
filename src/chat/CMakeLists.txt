add_library(lantalk_chat STATIC
    message.cpp
    message_queue.cpp
    chat_session.cpp
)

target_include_directories(lantalk_chat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(lantalk_chat
    PUBLIC
        lantalk_network
        lantalk_logging
        lantalk_config
)
