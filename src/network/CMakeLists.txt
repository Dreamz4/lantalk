add_library(lantalk_network STATIC
    tcp_socket.cpp
    tcp_server.cpp
    tcp_client.cpp
    connection.cpp
    protocol.cpp
)

target_include_directories(lantalk_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(lantalk_network
    PUBLIC
        lantalk_logging
        lantalk_config
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)

if(WIN32)
    target_link_libraries(lantalk_network PUBLIC ws2_32 wsock32 crypt32)
endif()
