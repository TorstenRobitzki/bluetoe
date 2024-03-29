/*
 * Example to prove, that the nrf52 binding is robust to miss some connection events due to
 * high CPU load.
 */

#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include <bluetoe/gatt_options.hpp>
#include <nrf.h>

using namespace bluetoe;

// LED1 on a nRF52 eval board
static constexpr int io_pin = 30;

static std::uint8_t io_pin_write_handler( bool state )
{
    // on an nRF52 eval board, the pin is connected to the LED's cathode, this inverts the logic.
    NRF_GPIO->OUT = state
        ? NRF_GPIO->OUT & ~( 1 << io_pin )
        : NRF_GPIO->OUT | ( 1 << io_pin );

    return error_codes::success;
}

using blinky_server = server<
    service<
        service_uuid< 0xC11169E1, 0x6252, 0x4450, 0x931C, 0x1B43A318783B >,
        characteristic<
            requires_encryption,
            free_write_handler< bool, io_pin_write_handler >
        >
    >,
    max_mtu_size< 65 >
>;

static constexpr std::uint32_t in_callback_io_pin = 30;
static NRF_TIMER_Type& delay_timer = *NRF_TIMER1;

static void init_hardware()
{
    NRF_GPIO->PIN_CNF[ in_callback_io_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

    NRF_GPIO->OUTCLR = ( 1 << in_callback_io_pin );

    delay_timer.PRESCALER = 4; // results in a clock of 16Mz / 2^4 == 1MHz
    delay_timer.BITMODE   = TIMER_BITMODE_BITMODE_32Bit;
    delay_timer.SHORTS    =
        ( TIMER_SHORTS_COMPARE0_STOP_Enabled << TIMER_SHORTS_COMPARE0_STOP_Pos )
      | ( TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos );

    delay_timer.TASKS_STOP = 1;
    delay_timer.EVENTS_COMPARE[0] = 0;

    // disable cache
//    NRF_NVMC->ICACHECNF = 0;
    // Disable Blockprotection
//    NRF_BPROT->DISABLEINDEBUG = BPROT_DISABLEINDEBUG_DISABLEINDEBUG_Msk;
}

template < typename T >
struct load_connection_event_callback
{
    static void call_connection_event_callback( const bluetoe::link_layer::delta_time& time_till_next_event );

    using meta_type = bluetoe::link_layer::details::connection_event_callback_meta_type;
};


device<
    blinky_server,
    security_manager,
    bluetoe::nrf::calibrated_rc_sleep_clock,
    bluetoe::nrf::high_frequency_crystal_oscillator_startup_time< 1000 >,
    link_layer::buffer_sizes< 200, 200 >,
    link_layer::sleep_clock_accuracy_ppm< 500 >,
    load_connection_event_callback< blinky_server >
 > gatt_srv;

template < typename T >
void load_connection_event_callback< T >::call_connection_event_callback( const bluetoe::link_layer::delta_time& /* time_till_next_event */ )
{
    NRF_GPIO->OUTSET = ( 1 << in_callback_io_pin );

    static int count = 0;

    ++count;

    // every once in a while lets consume more CPU time than time_till_next_event
    if ( count == 5 )
    {
        delay_timer.CC[ 0 ] = 80 * 1000;
        delay_timer.TASKS_START = 1;

        for ( ; !delay_timer.EVENTS_COMPARE[0] ; )
            ;

        delay_timer.EVENTS_COMPARE[0] = 0;
    }
    // Or lets utilize / stop the CPU by eraseing a flash page
    else if ( count == 100 )
    {
        count = 0;
        gatt_srv.nrf_flash_memory_access_begin();

        while ( NRF_NVMC->READY == NVMC_READY_READY_Busy )
            ;

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
        __ISB();
        __DSB();

        NRF_NVMC->ERASEPAGE = 0x20000;

        while ( NRF_NVMC->READY == NVMC_READY_READY_Busy )
            ;

        gatt_srv.nrf_flash_memory_access_end();
    }

    NRF_GPIO->OUTCLR = ( 1 << in_callback_io_pin );
}

int main()
{
    init_hardware();

    // Init GPIO pin
    NRF_GPIO->PIN_CNF[ io_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

    for ( ;; )
        gatt_srv.run();
}
