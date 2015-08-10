#include <bluetoe/bindings/nrf51.hpp>

#include <nrf.h>

#include <cassert>
#include <cstdint>
#include <algorithm>

namespace bluetoe {
namespace nrf51_details {

    static constexpr NRF_RADIO_Type*    nrf_radio            = NRF_RADIO;
    static constexpr NRF_TIMER_Type*    nrf_timer            = NRF_TIMER0;
    static constexpr NVIC_Type*         nvic                 = NVIC;
    static scheduled_radio_base*        instance             = nullptr;
    // the timeout timer will be canceled when the address is received; that's after T_IFS (150µs +- 2) 5 Bytes and some addition 20µs
    static constexpr std::uint32_t      reponse_timeout_us   = 152 + 5 * 8 + 20;
    static constexpr std::uint8_t       maximum_advertising_pdu_size = 0x3f;
    static constexpr std::size_t        radio_end_capture2_ppi_channel = 27;
    static constexpr std::size_t        compare0_rxen_ppi_channel = 21;
    static constexpr std::size_t        compare1_disable_ppi_channel = 22;

    static void toggle_debug_pins()
    {
        NRF_GPIO->OUT = NRF_GPIO->OUT ^ ( 1 << 18 );
        NRF_GPIO->OUT = NRF_GPIO->OUT ^ ( 1 << 19 );
    }

    static void toggle_debug_pin1()
    {
        NRF_GPIO->OUT = NRF_GPIO->OUT ^ ( 1 << 18 );
    }

    static void toggle_debug_pin2()
    {
        NRF_GPIO->OUT = NRF_GPIO->OUT ^ ( 1 << 19 );
    }

    static void init_debug_pins()
    {
        NRF_GPIO->PIN_CNF[ 18 ] =
            ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
            ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

        NRF_GPIO->PIN_CNF[ 19 ] =
            ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
            ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

        toggle_debug_pins();
        toggle_debug_pin1();
        toggle_debug_pin2();
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

        NRF_RADIO->TXADDRESS = 0;
        NRF_RADIO->RXADDRESSES = 1 << 0;

        NRF_RADIO->CRCCNF    =
            ( RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos ) |
            ( RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos );

        // capture timer0 in CC[ 2 ] with every end event. This is used to define the first anchor when switching
        // from advertising to connecting
        NRF_PPI->CHENSET = 1 << radio_end_capture2_ppi_channel;

        // The polynomial has the form of x^24 +x^10 +x^9 +x^6 +x^4 +x^3 +x+1
        NRF_RADIO->CRCPOLY   = 0x100065B;

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
        nrf_timer->EVENTS_COMPARE[ 1 ] = 0;
        nrf_timer->INTENCLR    = 0xffffffff;

        nrf_timer->TASKS_START = 1;
    }

    // see https://devzone.nordicsemi.com/question/47493/disable-interrupts-and-enable-interrupts-if-they-where-enabled/
    scheduled_radio_base::lock_guard::lock_guard()
        : context_( __get_PRIMASK() )
    {
        __disable_irq();
    }

    scheduled_radio_base::lock_guard::~lock_guard()
    {
        __set_PRIMASK( context_ );
    }

    scheduled_radio_base::scheduled_radio_base( adv_callbacks& cbs )
        : callbacks_( cbs )
        , timeout_( false )
        , received_( false )
        , state_( state::idle )
        , last_advertising_( false )
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

    void scheduled_radio_base::schedule_advertisment_and_receive(
            unsigned channel,
            const link_layer::write_buffer& transmit, link_layer::delta_time when,
            const link_layer::read_buffer& receive )
    {
        assert( ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) == RADIO_STATE_STATE_Disabled );
        assert( !received_ );
        assert( !timeout_ );
        assert( state_ == state::idle );
        assert( receive.buffer && receive.size >= 2u || receive.empty() );

        last_advertising_ = true;
        const unsigned      frequency  = frequency_from_channel( channel );
        const std::uint8_t  send_size  = std::min< std::size_t >( transmit.size, maximum_advertising_pdu_size );

        receive_buffer_      = receive;
        receive_buffer_.size = std::min< std::size_t >( receive.size, maximum_advertising_pdu_size );

        NRF_RADIO->FREQUENCY   = frequency;
        NRF_RADIO->DATAWHITEIV = channel & 0x3F;
        NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit.buffer );
        NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( send_size << RADIO_PCNF1_MAXLEN_Pos );

        NRF_RADIO->INTENCLR    = 0xffffffff;

        NRF_RADIO->EVENTS_END       = 0;
        NRF_RADIO->EVENTS_DISABLED  = 0;
        NRF_RADIO->EVENTS_READY     = 0;
        NRF_RADIO->EVENTS_ADDRESS   = 0;
        NRF_RADIO->EVENTS_PAYLOAD   = 0;

        NRF_RADIO->SHORTS      =
            RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

        NRF_PPI->CHENCLR = ( 1 << compare0_rxen_ppi_channel ) | ( 1 << compare1_disable_ppi_channel );

        NRF_RADIO->INTENSET    = RADIO_INTENSET_DISABLED_Msk | RADIO_INTENSET_PAYLOAD_Msk;

        if ( when.zero() )
        {
            state_ = state::adv_transmitting;
            NRF_RADIO->TASKS_TXEN = 1;
        }
        else
        {
            state_ = state::adv_transmitting_pending;

            nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
            nrf_timer->CC[0]               = when.usec();

            // manually triggering event for timer beeing already behind target time
            nrf_timer->TASKS_CAPTURE[ 1 ]  = 1;
            nrf_timer->INTENSET            = TIMER_INTENSET_COMPARE0_Msk;

            if ( nrf_timer->EVENTS_COMPARE[ 0 ] || nrf_timer->CC[ 1 ] >= nrf_timer->CC[ 0 ] )
            {
                state_ = state::adv_transmitting;
                nrf_timer->TASKS_CLEAR = 1;
                NRF_RADIO->TASKS_TXEN = 1;
            }
        }
    }

    void scheduled_radio_base::set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
    {
        NRF_RADIO->BASE0     = ( access_address << 8 ) & 0xFFFFFF00;
        NRF_RADIO->PREFIX0   = ( access_address >> 24 ) & RADIO_PREFIX0_AP0_Msk;
        NRF_RADIO->CRCINIT   = crc_init;
    }

    void scheduled_radio_base::run()
    {
        // TODO send cpu to sleep
        while ( !received_ && !timeout_ )
            ;

        // when either received_ or timeout_ is true, no timer should be scheduled and the radio should be idle
        assert( ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) == RADIO_STATE_STATE_Disabled );
        assert( nrf_timer->INTENCLR == 0 );

        if ( received_ )
        {
            assert( reinterpret_cast< std::uint8_t* >( NRF_RADIO->PACKETPTR ) == receive_buffer_.buffer );

            receive_buffer_.size = std::min< std::size_t >( receive_buffer_.size, ( receive_buffer_.buffer[ 1 ] & 0x3f ) + 2 );
            received_ = false;

            callbacks_.adv_received( receive_buffer_ );
        }
        else
        {
            timeout_ = false;
            callbacks_.adv_timeout();
        }
    }

    void scheduled_radio_base::radio_interrupt()
    {
        if ( NRF_RADIO->EVENTS_PAYLOAD )
        {
            NRF_RADIO->EVENTS_PAYLOAD = 0;

            if ( state_ == state::adv_transmitting && !receive_buffer_.empty() )
            {
                NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer_.buffer );
                NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer_.size << RADIO_PCNF1_MAXLEN_Pos );

                NRF_RADIO->EVENTS_ADDRESS      = 0;
                NRF_RADIO->INTENSET            = RADIO_INTENSET_ADDRESS_Msk;
                NRF_RADIO->INTENCLR            = RADIO_INTENSET_PAYLOAD_Msk;
            }
        }

        if ( NRF_RADIO->EVENTS_DISABLED )
        {
            NRF_RADIO->EVENTS_DISABLED = 0;

            if ( state_ == state::adv_timeout_stopping )
            {
                state_ = state::idle;

                NRF_RADIO->INTENCLR    = 0xffffffff;
                nrf_timer->INTENCLR    = 0xffffffff;

                timeout_ = true;
            }
            else if ( state_ == state::adv_transmitting && receive_buffer_.empty() )
            {
                state_ = state::idle;
                timeout_ = true;
            }
            else if ( state_ == state::adv_transmitting && !receive_buffer_.empty() )
            {
                NRF_RADIO->TASKS_RXEN          = 1;
                state_ = state::adv_receiving;

                nrf_timer->TASKS_CAPTURE[ 0 ]  = 1;
                nrf_timer->CC[0]              += reponse_timeout_us;
                nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
                nrf_timer->INTENSET            = TIMER_INTENSET_COMPARE0_Msk;
            }
            else if ( state_ == state::adv_receiving )
            {
                state_ = state::idle;

                nrf_timer->INTENCLR            = TIMER_INTENSET_COMPARE0_Msk;
                nrf_timer->EVENTS_COMPARE[ 0 ] = 0;

                if ( ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) == RADIO_CRCSTATUS_CRCSTATUS_CRCOk )
                {
                    received_  = true;
                }
                else
                {
                    timeout_ = true;
                }
            }
        }

        if ( NRF_RADIO->EVENTS_ADDRESS )
        {
            NRF_RADIO->EVENTS_ADDRESS  = 0;

            if ( state_ == state::adv_receiving )
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
        nrf_timer->INTENCLR            = TIMER_INTENSET_COMPARE0_Msk;
        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;

        if ( state_ == state::adv_receiving )
        {
            state_ = state::adv_timeout_stopping;

            NRF_RADIO->TASKS_DISABLE = 1;
        }
        else if ( state_ == state::adv_transmitting_pending )
        {
            state_ = state::adv_transmitting;

            nrf_timer->TASKS_CLEAR = 1;
            NRF_RADIO->TASKS_TXEN  = 1;
        }
    }

    std::uint32_t scheduled_radio_base::static_random_address_seed() const
    {
        return NRF_FICR->DEVICEID[ 0 ];
    }

    void scheduled_radio_base::start_connection_event(
        unsigned                        channel,
        bluetoe::link_layer::delta_time start_receive,
        bluetoe::link_layer::delta_time end_receive,
        const link_layer::read_buffer&  receive_buffer )
    {
        assert( ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) == RADIO_STATE_STATE_Disabled );
        assert( state_ == state::idle );
        assert( receive_buffer.buffer && receive_buffer.size >= 2u || receive_buffer.empty() );
        assert( start_receive < end_receive );

        state_ = state::evt_wait_connect;

        const unsigned      frequency  = frequency_from_channel( channel );

        NRF_RADIO->FREQUENCY   = frequency;
        NRF_RADIO->DATAWHITEIV = channel & 0x3F;
        NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer.buffer );
        NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer.size << RADIO_PCNF1_MAXLEN_Pos );

        NRF_RADIO->INTENCLR    = 0xffffffff;
        nrf_timer->INTENCLR    = 0xffffffff;

        NRF_RADIO->EVENTS_END       = 0;
        NRF_RADIO->EVENTS_DISABLED  = 0;
        NRF_RADIO->EVENTS_READY     = 0;
        NRF_RADIO->EVENTS_ADDRESS   = 0;

        NRF_RADIO->SHORTS      =
            RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

        // Interrupt on Disable event
        NRF_RADIO->INTENSET    = RADIO_INTENSET_DISABLED_Msk;

        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 1 ] = 0;

        // If last_advertising_ is set, there was no previous connection event, but an advertising event.
        // CC[ 2 ] contains the time of the last radio end event, thus the anchor for the connection
        if ( last_advertising_ )
        {
            last_advertising_ = false;
            anchor_offset_ = link_layer::delta_time( nrf_timer->CC[ 2 ] );
        }

        nrf_timer->CC[ 0 ]             = start_receive.usec() - anchor_offset_.usec();
        nrf_timer->CC[ 1 ]             = end_receive.usec() - anchor_offset_.usec();

        NRF_PPI->CHENSET = ( 1 << compare0_rxen_ppi_channel ) | ( 1 << compare1_disable_ppi_channel );

        // manually triggering event for timer beeing already behind target time
        nrf_timer->TASKS_CAPTURE[ 2 ]  = 1;

        if ( nrf_timer->CC[ 2 ] >= nrf_timer->CC[ 0 ] )
            nrf_timer->EVENTS_COMPARE[ 0 ] = 1;

        if ( nrf_timer->CC[ 2 ] >= nrf_timer->CC[ 1 ] )
            nrf_timer->EVENTS_COMPARE[ 1 ] = 1;

        nrf_timer->INTENSET            = TIMER_INTENSET_COMPARE1_Msk;
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