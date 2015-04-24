#include <cstdint>

#include <iostream>
#include <ostream>

#include "btstack-config.h"

#include <btstack/run_loop.h>

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include <btstack/hci_cmds.h>

#include <bluetoe/server.hpp>
#include <bluetoe/bindings/btstack_libusb.hpp>

std::uint32_t temperature_value = 0x12345678;
static const char server_name[] = "Temperature";

typedef bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
            bluetoe::no_write_access
        >
    >
> small_temperature_service;


int main()
{
    small_temperature_service                                   server;
    bluetoe::btstack_libusb_device< small_temperature_service > device;

    device.run( server );
}
