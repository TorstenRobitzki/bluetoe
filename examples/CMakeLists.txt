
set(BTSTACK_INCLUDE_DIR ~/btstack/include/ ~/btstack/platforms/libusb ~/btstack/src ~/btstack/ble)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")

include_directories(${BTSTACK_INCLUDE_DIR} ${LIBUSB_1_INCLUDE_DIR})
add_executable(btstack_example btstack_example.cpp)
target_link_libraries(btstack_example ${LIBUSB_1_LIBRARY} btstack_libusb)

add_executable(clock clock.cpp)
target_link_libraries(clock ${LIBUSB_1_LIBRARY} btstack_libusb)

add_subdirectory(doxygen)
