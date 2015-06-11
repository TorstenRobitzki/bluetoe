#include <bluetoe/bindings/nrf51.hpp>

#include <nrf.h>

#include <cassert>
#include <cstdint>

namespace bluetoe {
namespace nrf51_details {

    static constexpr std::uint32_t      radio_access_address = 0x8E89BED6;
    static constexpr NRF_RADIO_Type*    nrf_radio            = NRF_RADIO;

    static void init_debug_pins()
    {
        NRF_GPIO->PIN_CNF[ 18 ] =
            ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
            ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

        NRF_GPIO->PIN_CNF[ 19 ] =
            ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
            ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );
    }

    static void init_radio()
    {
        if ( ( NRF_FICR->OVERRIDEEN & FICR_OVERRIDEEN_BLE_1MBIT_Msk ) == (FICR_OVERRIDEEN_BLE_1MBIT_Override << FICR_OVERRIDEEN_BLE_1MBIT_Pos) )
        {
            NRF_RADIO->OVERRIDE0 = NRF_FICR->BLE_1MBIT[0];
            NRF_RADIO->OVERRIDE1 = NRF_FICR->BLE_1MBIT[1];
            NRF_RADIO->OVERRIDE2 = NRF_FICR->BLE_1MBIT[2];
            NRF_RADIO->OVERRIDE3 = NRF_FICR->BLE_1MBIT[3];
            NRF_RADIO->OVERRIDE4 = NRF_FICR->BLE_1MBIT[4] | 0x80000000;
        }

        NRF_RADIO->MODE  = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;

        NRF_RADIO->PCNF0 =
            ( 1 << RADIO_PCNF0_S0LEN_Pos ) |
            ( 8 << RADIO_PCNF0_LFLEN_Pos ) |
            ( 0 << RADIO_PCNF0_S1LEN_Pos );

        NRF_RADIO->PCNF1 =
            ( RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos ) |
            ( RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos ) |
            ( 3 << RADIO_PCNF1_BALEN_Pos ) |
            ( 0 << RADIO_PCNF1_STATLEN_Pos );

        NRF_RADIO->BASE0     = ( radio_access_address << 8 ) & 0xFFFFFF00;
        NRF_RADIO->PREFIX0   = ( radio_access_address >> 24 ) & RADIO_PREFIX0_AP0_Msk;
        NRF_RADIO->TXADDRESS = 0;

        NRF_RADIO->CRCCNF    =
            ( RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos ) |
            ( RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos );

        // The polynomial has the form of x^24 +x^10 +x^9 +x^6 +x^4 +x^3 +x+1
        NRF_RADIO->CRCPOLY   = 0x100065B;
        NRF_RADIO->CRCINIT   = 0x555555;

        NRF_RADIO->TIFS      = 150;
    }

    static void init_timer()
    {
        NRF_TIMER0->MODE        = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
        NRF_TIMER0->BITMODE     = TIMER_BITMODE_BITMODE_32Bit;
        NRF_TIMER0->PRESCALER   = 4; // resulting in a timer resolution of 1µs

        NRF_TIMER0->TASKS_STOP  = 1;
        NRF_TIMER0->TASKS_CLEAR = 1;
        NRF_TIMER0->EVENTS_COMPARE[ 0 ] = 0;

        NRF_TIMER0->INTENSET    = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
    }

    scheduled_radio_base::scheduled_radio_base( callbacks& cbs )
        : callbacks_( cbs )
    {
        // start high freuquence clock source if not done yet
        if ( !NRF_CLOCK->EVENTS_HFCLKSTARTED )
        {
            NRF_CLOCK->TASKS_HFCLKSTART = 1;

            while ( !NRF_CLOCK->EVENTS_HFCLKSTARTED )
                ;
        }

        init_debug_pins();
        init_radio();
        init_timer();

        NVIC_ClearPendingIRQ( RADIO_IRQn );
        NVIC_EnableIRQ( RADIO_IRQn );
        NVIC_ClearPendingIRQ( TIMER0_IRQn );
        NVIC_EnableIRQ( TIMER0_IRQn );
    }

    void scheduled_radio_base::schedule_transmit_and_receive(
            unsigned channel,
            const link_layer::write_buffer& transmit, link_layer::delta_time when,
            const link_layer::read_buffer& receive, link_layer::delta_time timeout )
    {
    }


}
}