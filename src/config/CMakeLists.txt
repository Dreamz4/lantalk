add_library(lantalk_config STATIC config.cpp)

target_include_directories(lantalk_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(lantalk_config PUBLIC lantalk_logging OpenSSL::Crypto)
