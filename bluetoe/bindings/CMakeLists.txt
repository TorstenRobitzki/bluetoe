find_path(LIBUSB_1_INCLUDE_DIR
    NAMES
        libusb.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
        /sw/include
    PATH_SUFFIXES
        libusb-1.0
)

find_library(LIBUSB_1_LIBRARY
    NAMES
        usb-1.0 usb
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        /sw/lib
)

set(BTSTACK_INCLUDE_DIR ~/btstack/include/ ~/btstack/platforms/libusb ~/btstack/src ~/btstack/ble)

include_directories(${BTSTACK_INCLUDE_DIR} ${LIBUSB_1_INCLUDE_DIR})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format-extra-args -Wno-gnu-zero-variadic-macro-arguments")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")

add_library(btstack_libusb btstack_libusb.cpp ~/btstack/src/btstack_memory.c ~/btstack/src/hci.c ~/btstack/src/hci_cmds.c ~/btstack/src/hci_dump.c ~/btstack/platforms/posix/src/hci_transport_h2_libusb.c ~/btstack/platforms/posix/src/remote_device_db_fs.c ~/btstack/src/utils.c ~/btstack/src/sdp_util.c ~/btstack/src/linked_list.c ~/btstack/src/run_loop.c ~//btstack/platforms/posix/src/run_loop_posix.c)