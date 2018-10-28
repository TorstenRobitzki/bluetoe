add_library(test_tools STATIC
        address_io.cpp
        test_uuid.cpp
        test_servers.cpp
        hexdump.cpp
        test_radio.cpp
        buffer_io.cpp)
add_library(test::tools ALIAS test_tools)

target_include_directories(test_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_tools PRIVATE bluetoe::linklayer)
target_compile_features(test_tools PRIVATE cxx_std_11)
target_compile_options(test_tools PRIVATE -Wall -pedantic -Wextra -Wfatal-errors)
target_compile_definitions(test_tools PRIVATE "")


