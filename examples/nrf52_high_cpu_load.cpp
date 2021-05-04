/*
 * Example to prove, that the nrf52 binding is robust to miss some connection events due to
 * high CPU load.
 */

#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include <nrf.h>

using namespace bluetoe;

// LED1 on a nRF52 eval board
static constexpr int io_pin = 17;

static std::uint8_t io_pin_write_handler( bool state )
{
    // on an nRF52 eval board, the pin is connected to the LED's cathode, this inverts the logic.
    NRF_GPIO->OUT = state
        ? NRF_GPIO->OUT & ~( 1 << io_pin )
        : NRF_GPIO->OUT | ( 1 << io_pin );

    return error_codes::success;
}

typedef server<
    service<
        service_uuid< 0xC11169E1, 0x6252, 0x4450, 0x931C, 0x1B43A318783B >,
        characteristic<
            requires_encryption,
            free_write_handler< bool, io_pin_write_handler >
        >
    >
> blinky_server;

static constexpr std::uint32_t in_callback_io_pin = 25;
static NRF_TIMER_Type& delay_timer = *NRF_TIMER1;

static void init_hardware()
{
    NRF_GPIO->PIN_CNF[ in_callback_io_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

    delay_timer.PRESCALER = 4; // results in a clock of 16Mz / 2^4 == 1MHz
    delay_timer.BITMODE   = TIMER_BITMODE_BITMODE_32Bit;
    delay_timer.SHORTS    =
        ( TIMER_SHORTS_COMPARE0_STOP_Enabled << TIMER_SHORTS_COMPARE0_STOP_Pos )
      | ( TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos );

    delay_timer.TASKS_STOP = 1;
    delay_timer.EVENTS_COMPARE[0] = 0;
}

template < typename T, T& Obj >
struct load_connection_event_callback
{
    static void call_connection_event_callback( const bluetoe::link_layer::delta_time& time_till_next_event )
    {
        NRF_GPIO->OUTSET = ( 1 << in_callback_io_pin );
        static int count = 0;

        ++count;

        // every once in a while lets consume more CPU time than time_till_next_event
        if ( count == 5 )
        {
            count = 0;
            delay_timer.CC[ 0 ] = 80 * 1000;
            delay_timer.TASKS_START = 1;

            for ( ; !delay_timer.EVENTS_COMPARE[0] ; )
                ;

            delay_timer.EVENTS_COMPARE[0] = 0;
        }

        NRF_GPIO->OUTCLR = ( 1 << in_callback_io_pin );
    }

    typedef bluetoe::link_layer::details::connection_event_callback_meta_type meta_type;
};

blinky_server gatt;

device<
    blinky_server,
    load_connection_event_callback< blinky_server, gatt >
 > gatt_srv;

int main()
{
    init_hardware();

    // Init GPIO pin
    NRF_GPIO->PIN_CNF[ io_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

    for ( ;; )
        gatt_srv.run( gatt );
}
