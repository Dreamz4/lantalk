add_library(lantalk_logging STATIC logger.cpp)

target_include_directories(lantalk_logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(lantalk_logging PUBLIC Threads::Threads)
