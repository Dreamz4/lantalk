cmake_minimum_required(VERSION 3.20)

enable_testing()
find_package(GTest QUIET)

if(GTest_FOUND)
    # Use GTest
else()
    # Simple custom test framework using assert
endif()

# Create test executables
add_executable(test_protocol test_protocol.cpp)
# target_link_libraries(test_protocol PRIVATE lantalk_network lantalk_logging lantalk_config)
add_test(NAME test_protocol COMMAND test_protocol)

add_executable(test_message test_message.cpp)
# target_link_libraries(test_message PRIVATE lantalk_chat lantalk_logging lantalk_config lantalk_network)
add_test(NAME test_message COMMAND test_message)

add_executable(test_message_queue test_message_queue.cpp)
# target_link_libraries(test_message_queue PRIVATE lantalk_chat lantalk_logging lantalk_config lantalk_network)
add_test(NAME test_message_queue COMMAND test_message_queue)

add_executable(test_config test_config.cpp)
# target_link_libraries(test_config PRIVATE lantalk_config lantalk_logging)
add_test(NAME test_config COMMAND test_config)

add_executable(test_discovery test_discovery.cpp)
# target_link_libraries(test_discovery PRIVATE lantalk_discovery lantalk_config lantalk_logging)
add_test(NAME test_discovery COMMAND test_discovery)
