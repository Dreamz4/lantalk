cmake_minimum_required(VERSION 3.20)
project(LanTalk VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

if(WIN32)
    add_compile_definitions(LANTALK_PLATFORM_WINDOWS=1)
elseif(APPLE)
    add_compile_definitions(LANTALK_PLATFORM_MACOS=1)
elseif(UNIX)
    add_compile_definitions(LANTALK_PLATFORM_LINUX=1)
endif()

add_subdirectory(src/logging)
add_subdirectory(src/config)
add_subdirectory(src/network)
add_subdirectory(src/chat)
add_subdirectory(src/discovery)
add_subdirectory(src/security)
add_subdirectory(src/cli)

add_executable(lantalk src/main.cpp)
target_link_libraries(lantalk PRIVATE 
    lantalk_logging 
    lantalk_config 
    lantalk_network 
    lantalk_chat 
    lantalk_discovery 
    lantalk_security 
    lantalk_cli
    OpenSSL::SSL 
    OpenSSL::Crypto 
    Threads::Threads
)

if(WIN32)
    target_link_libraries(lantalk PRIVATE ws2_32 wsock32 crypt32)
endif()

option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS lantalk DESTINATION bin)
