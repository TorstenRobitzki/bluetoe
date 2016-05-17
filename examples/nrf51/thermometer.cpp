#include <bluetoe/bindings/nrf51.hpp>
#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/server.hpp>

#include <nrf.h>

std::int32_t temperature_value = 0x12345678;
static constexpr char server_name[] = "Temperature";
static constexpr char char_name[] = "Temperature Value";

typedef bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0000, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xF0E6EBE6, 0x3749, 0x41A6, 0xB190, 0x591B262AC20A >,
            bluetoe::no_write_access,
            bluetoe::notify,
            bluetoe::characteristic_name< char_name >,
            bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >
        >
    >
> small_temperature_service;

void start_temperatur_messurement();

small_temperature_service                   gatt;
bluetoe::nrf51<
    small_temperature_service,
    bluetoe::link_layer::advertising_interval< 250u >,
    bluetoe::link_layer::static_address< 0xf0, 0xa6, 0xd5, 0x4f, 0x60, 0x0b >
> server;

int main()
{
    NVIC_SetPriority( TEMP_IRQn, 3 );
    NVIC_ClearPendingIRQ( TEMP_IRQn );
    NVIC_EnableIRQ( TEMP_IRQn );
    NRF_TEMP->INTENSET    = TEMP_INTENSET_DATARDY_Set;
    NRF_TEMP->TASKS_START = 1;

    for ( ;; )
        server.run( gatt );
}

extern "C" void TEMP_IRQHandler(void)
{
    NRF_TEMP->EVENTS_DATARDY = 0;

    if ( NRF_TEMP->TEMP != temperature_value )
    {
        temperature_value = NRF_TEMP->TEMP;
        gatt.notify( temperature_value );
    }

    NRF_TEMP->TASKS_START = 1;
}