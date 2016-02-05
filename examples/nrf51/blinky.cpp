/*
 * Very simple example to expose an IO pin via BLE
 */
#include <bluetoe/server.hpp>
#include <bluetoe/bindings/nrf51.hpp>
#include <nrf.h>

static constexpr int io_pin = 19;

static std::uint8_t characteristic_write_handler(std::size_t write_size, const std::uint8_t *value)
{
    if ( write_size != 1 )
        return bluetoe::error_codes::invalid_attribute_value_length;

    if ( *value != 1 && *value != 0 )
        return bluetoe::error_codes::out_of_range;

    // the GPIO pin according to the received value: 0 = off, 1 = on
    NRF_GPIO->OUT = *value
        ? NRF_GPIO->OUT | ( 1 << io_pin )
        : NRF_GPIO->OUT & ~( 1 << io_pin );

    return bluetoe::error_codes::success;
}

typedef bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0xC11169E1, 0x6252, 0x4450, 0x931C, 0x1B43A318783B >,
        bluetoe::characteristic<
            bluetoe::free_write_handler< characteristic_write_handler >
        >
    >
> blinky_server;

blinky_server gatt;

bluetoe::nrf51< blinky_server > server;

int main()
{
    // Init GPIO pin
    NRF_GPIO->PIN_CNF[ io_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

    for ( ;; )
        server.run( gatt );
}
