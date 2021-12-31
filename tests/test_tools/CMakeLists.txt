find_package( Boost REQUIRED )

enable_language(C)

add_library(test_tools STATIC
        aes.c
        address_io.cpp
        attribute_io.cpp
        test_uuid.cpp
        test_servers.cpp
        hexdump.cpp
        test_radio.cpp
        buffer_io.cpp
        pairing_status_io.cpp
        uECC.c)

add_library(test::tools ALIAS test_tools)

target_include_directories(test_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(test_tools PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(test_tools PRIVATE bluetoe::link_layer bluetoe::utility bluetoe::iface)
target_compile_features(test_tools PRIVATE cxx_std_11)
target_compile_definitions(test_tools PRIVATE "")

set_property(TARGET test_tools PROPERTY C_STANDARD 99)
set_source_files_properties(uECC.c PROPERTIES COMPILE_FLAGS -Wno-unused-variable)
set_source_files_properties(uECC.c PROPERTIES COMPILE_DEFINITIONS uECC_CURVE=uECC_secp256r1)
