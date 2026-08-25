add_library(lantalk_security STATIC
    crypto.cpp
    authentication.cpp
)

target_include_directories(lantalk_security PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(lantalk_security
    PUBLIC
        lantalk_logging
        lantalk_config
        OpenSSL::SSL
        OpenSSL::Crypto
)
