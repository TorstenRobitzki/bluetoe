add_library(bluetoe_bindings_hci_libusb STATIC libusb.cpp)
add_library(bluetoe::bindings::hci_libusb ALIAS bluetoe_bindings_hci_libusb)

target_include_directories(bluetoe_bindings_hci_libusb
    INTERFACE
        include_libusb)

target_link_libraries(bluetoe_bindings_hci_libusb
    PUBLIC
        bluetoe::hci
        bluetoe::utility
        bluetoe::linklayer
)
