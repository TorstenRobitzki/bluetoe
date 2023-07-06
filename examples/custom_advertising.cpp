/**
 * @example purpose of this example is, to show how any required advertising data of scan response can
 *          be archived, but circumventing all automatism provided by Bluetoe.
 */

#include <bluetoe/server.hpp>
#include <bluetoe/custom_advertising.hpp>
#include <bluetoe/device.hpp>

using namespace bluetoe;

static const std::uint8_t blob[] = {
    'H', 'a', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'
};

static const std::uint8_t advertising_data[] = {
    0x02, 0x01, 0x06,           // «Flags» , LE General Discoverable Mode, BR/EDR Not Supported.
    0x06, 0xff,                 // «Manufacturer Specific Data»
    0x69, 0x02,                 // Torrox
    0x22, 0x23, 0x44,           // Data
    0x02, 0x0A, 0x00,           // «TX Power Level»
};

static const std::uint8_t scan_response_data[] = {
    0x03, 0x19, 0xC1, 0x03,     // «Appearance» Keyboard
};

using advertising_server = server<
    custom_advertising_data< sizeof( advertising_data ), advertising_data >,
    custom_scan_response_data< sizeof( scan_response_data ), scan_response_data >,
    service<
        service_uuid< 0x52CBA985, 0x7E3C, 0x4ECD, 0xBDF1, 0x2708157E080B >,
        characteristic<
            fixed_blob_value< blob, sizeof( blob ) >
        >
    >
>;

device< advertising_server > gatt_srv;

int main()
{
    for ( ;; )
        gatt_srv.run();
}
