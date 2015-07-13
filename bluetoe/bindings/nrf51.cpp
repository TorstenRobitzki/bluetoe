#include <bluetoe/bindings/nrf51.hpp>

#include <nrf.h>

#include <cassert>
#include <cstdint>
#include <algorithm>

namespace bluetoe {
namespace nrf51_details {

    static constexpr std::uint32_t      radio_access_address = 0x8E89BED6;
    static constexpr NRF_RADIO_Type*    nrf_radio            = NRF_RADIO;
    static constexpr NRF_TIMER_Type*    nrf_timer            = NRF_TIMER0;
    static constexpr NVIC_Type*         nvic                 = NVIC;
    static scheduled_radio_base*        instance             = nullptr;
    // the timeout timer will be canceled when the address is received; that's after T_IFS (150µs +- 2) 5 Bytes and some addition 20µs
    static constexpr std::uint32_t      reponse_timeout_us   = 152 + 5 * 8 + 20;

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
        nrf_timer->MODE        = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
        nrf_timer->BITMODE     = TIMER_BITMODE_BITMODE_32Bit;
        nrf_timer->PRESCALER   = 4; // resulting in a timer resolution of 1µs

        nrf_timer->TASKS_STOP  = 1;
        nrf_timer->TASKS_CLEAR = 1;
        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
        nrf_timer->INTENCLR    = 0xffffffff;

        nrf_timer->TASKS_START = 1;
    }

    scheduled_radio_base::scheduled_radio_base( callbacks& cbs )
        : callbacks_( cbs )
        , timeout_( false )
        , received_( false )
        , state_( state::idle )
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

        instance = this;

        NVIC_ClearPendingIRQ( RADIO_IRQn );
        NVIC_EnableIRQ( RADIO_IRQn );
        NVIC_ClearPendingIRQ( TIMER0_IRQn );
        NVIC_EnableIRQ( TIMER0_IRQn );
    }

    unsigned scheduled_radio_base::frequency_from_channel( unsigned channel ) const
    {
        assert( channel < 40 );

        if ( channel <= 10 )
            return 4 + 2 * channel;

        if ( channel <= 36 )
            return 6 + 2 * channel;

        if ( channel == 37 )
            return 2;

        if ( channel == 38 )
            return 26;

        return 80;
    }

    void scheduled_radio_base::schedule_transmit_and_receive(
            unsigned channel,
            const link_layer::write_buffer& transmit, link_layer::delta_time when,
            const link_layer::read_buffer& receive )
    {
        assert( ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) == RADIO_STATE_STATE_Disabled );
        assert( !received_ );
        assert( !timeout_ );
        assert( state_ == state::idle );

        const unsigned      frequency  = frequency_from_channel( channel );
        const std::uint8_t  send_size  = std::min< std::size_t >( transmit.size, 255 );

        receive_buffer_      = receive;
        receive_buffer_.size = std::min< std::size_t >( receive.size, 255 );

        NRF_RADIO->FREQUENCY   = frequency;
        NRF_RADIO->DATAWHITEIV = channel & 0x3F;
        NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit.buffer );
        NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( send_size << RADIO_PCNF1_MAXLEN_Pos );

        NRF_RADIO->EVENTS_END       = 0;
        NRF_RADIO->EVENTS_DISABLED  = 0;
        NRF_RADIO->EVENTS_READY     = 0;
        NRF_RADIO->EVENTS_ADDRESS   = 0;

        NRF_RADIO->INTENCLR    = 0xffffffff;

        NRF_RADIO->SHORTS      =
            RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

        NRF_RADIO->INTENSET    = RADIO_INTENSET_DISABLED_Msk;

        state_ = state::transmitting;

        if ( when.zero() )
        {
            NRF_RADIO->TASKS_TXEN = 1;
        }
    }

    void scheduled_radio_base::run()
    {
        // TODO send cpu to sleep
        while ( !received_ && !timeout_ )
            ;

        if ( received_ )
        {
            received_ = false;
            callbacks_.received( link_layer::read_buffer{ reinterpret_cast< std::uint8_t* >( NRF_RADIO->PACKETPTR ),  } );
        }
        else
        {
            timeout_ = false;
            callbacks_.timeout();
        }

        assert( !received_ );
        assert( !timeout_ );
    }

    void scheduled_radio_base::radio_interrupt()
    {
        if ( NRF_RADIO->EVENTS_DISABLED )
        {
            NRF_RADIO->EVENTS_DISABLED = 0;

            if ( state_ == state::timeout_stopping )
            {
                NRF_RADIO->INTENCLR    = 0xffffffff;
                nrf_timer->INTENCLR    = 0xffffffff;

                timeout_ = true;
                state_ = state::idle;
            }
            else if ( state_ == state::transmitting )
            {
                NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer_.buffer );
                NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer_.size << RADIO_PCNF1_MAXLEN_Pos );

                nrf_timer->TASKS_CAPTURE[ 0 ]  = 1;
                nrf_timer->CC[0]              += reponse_timeout_us;
                nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
                nrf_timer->INTENSET            = TIMER_INTENSET_COMPARE0_Msk;

                NRF_RADIO->EVENTS_ADDRESS      = 0;
                NRF_RADIO->INTENSET            = RADIO_INTENSET_ADDRESS_Msk;

                NRF_RADIO->TASKS_RXEN          = 1;

                state_ = state::receiving;
            }
            else if ( state_ == state::receiving )
            {
                nrf_timer->INTENCLR            = TIMER_INTENSET_COMPARE0_Msk;
                nrf_timer->EVENTS_COMPARE[ 0 ] = 0;

                received_  = true;
                state_ = state::idle;
            }
        }

        if ( NRF_RADIO->EVENTS_ADDRESS )
        {
            NRF_RADIO->EVENTS_ADDRESS  = 0;

            if ( state_ == state::receiving )
            {
                // dismantel timer, we are getting an end event now
                nrf_timer->INTENCLR            = TIMER_INTENSET_COMPARE0_Msk;
                nrf_timer->EVENTS_COMPARE[ 0 ] = 0;

                NRF_RADIO->INTENCLR            = RADIO_INTENSET_ADDRESS_Msk;
            }
        }
    }

    void scheduled_radio_base::timer_interrupt()
    {
        if ( state_ == state::receiving )
        {
            nrf_timer->INTENCLR      = TIMER_INTENSET_COMPARE0_Msk;
            NRF_RADIO->TASKS_DISABLE = 1;

            state_ = state::timeout_stopping;
        }
    }

    std::uint32_t scheduled_radio_base::static_random_address_seed() const
    {
        return NRF_FICR->DEVICEID[ 0 ];
    }

}
}

extern "C" void RADIO_IRQHandler(void)
{
    bluetoe::nrf51_details::instance->radio_interrupt();
}

extern "C" void TIMER0_IRQHandler(void)
{
    bluetoe::nrf51_details::instance->timer_interrupt();
}