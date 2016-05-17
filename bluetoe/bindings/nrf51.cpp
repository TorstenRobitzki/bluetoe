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
    static constexpr NRF_PPI_Type*      nrf_ppi              = NRF_PPI;
    static scheduled_radio_base*        instance             = nullptr;
    // after T_IFS (150µs +- 2) at maximum, a connection request will be received (34 Bytes + 1 Byte preable, 4 Bytes Access Address and 3 Bytes CRC)
    // plus some additional 20µs
    static constexpr std::uint32_t      adv_reponse_timeout_us   = 152 + 42 * 8 + 20;
    static constexpr std::uint8_t       maximum_advertising_pdu_size = 0x3f;

    static constexpr std::size_t        radio_address_capture1_ppi_channel = 26;
    static constexpr std::size_t        radio_end_capture2_ppi_channel = 27;
    static constexpr std::size_t        compare0_txen_ppi_channel = 20;
    static constexpr std::size_t        compare0_rxen_ppi_channel = 21;
    static constexpr std::size_t        compare1_disable_ppi_channel = 22;

    static constexpr std::uint8_t       more_data_flag = 0x10;

    static constexpr unsigned           us_from_packet_start_to_address_end = ( 1 + 4 ) * 8;
    static constexpr unsigned           us_radio_rx_startup_time            = 138;
    static constexpr unsigned           us_radio_tx_startup_time            = 140;
    static constexpr unsigned           connect_request_size                = 36;

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

    /*
     * Frequency correction if NRF_FICR_Type has a OVERRIDEEN field
     */
    template < typename F, typename R >
    static auto override_correction(F* ficr, R* radio) -> decltype(F::OVERRIDEEN)
    {
#       ifndef FICR_OVERRIDEEN_BLE_1MBIT_Msk
#           define FICR_OVERRIDEEN_BLE_1MBIT_Msk 1
#       endif
#       ifndef FICR_OVERRIDEEN_BLE_1MBIT_Override
#           define FICR_OVERRIDEEN_BLE_1MBIT_Override 1
#       endif
#       ifndef FICR_OVERRIDEEN_BLE_1MBIT_Pos
#           define FICR_OVERRIDEEN_BLE_1MBIT_Pos 1
#       endif

        if ( ( ficr->OVERRIDEEN & FICR_OVERRIDEEN_BLE_1MBIT_Msk ) == (FICR_OVERRIDEEN_BLE_1MBIT_Override << FICR_OVERRIDEEN_BLE_1MBIT_Pos) )
        {
            radio->OVERRIDE0 = ficr->BLE_1MBIT[0];
            radio->OVERRIDE1 = ficr->BLE_1MBIT[1];
            radio->OVERRIDE2 = ficr->BLE_1MBIT[2];
            radio->OVERRIDE3 = ficr->BLE_1MBIT[3];
            radio->OVERRIDE4 = ficr->BLE_1MBIT[4] | 0x80000000;
        }

        return ficr->OVERRIDEEN;
    }

    template < typename F >
    static void override_correction(...)
    {
    }

    static void init_radio()
    {
        override_correction< NRF_FICR_Type >( NRF_FICR, NRF_RADIO );

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

        // clear all used PPI pre-programmed channels (16.1.1)
        NRF_PPI->CHENCLR =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare0_rxen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

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
        nrf_timer->EVENTS_COMPARE[ 2 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 3 ] = 0;
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
        , evt_timeout_( false )
        , end_evt_( false )
        , wake_up_( 0 )
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

        NVIC_SetPriority( RADIO_IRQn, 0 );
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

        const std::uint8_t  send_size  = std::min< std::size_t >( transmit.size, maximum_advertising_pdu_size );

        receive_buffer_      = receive;
        receive_buffer_.size = std::min< std::size_t >( receive.size, maximum_advertising_pdu_size );
        if ( !receive_buffer_.empty() )
            receive_buffer_.buffer[ 1 ] = 0;

        NRF_RADIO->FREQUENCY   = frequency_from_channel( channel );
        NRF_RADIO->DATAWHITEIV = channel & 0x3F;
        NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit.buffer );
        NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( send_size << RADIO_PCNF1_MAXLEN_Pos );

        NRF_RADIO->INTENCLR    = 0xffffffff;
        nrf_timer->INTENCLR    = 0xffffffff;

        NRF_RADIO->EVENTS_END       = 0;
        NRF_RADIO->EVENTS_DISABLED  = 0;
        NRF_RADIO->EVENTS_READY     = 0;
        NRF_RADIO->EVENTS_ADDRESS   = 0;
        NRF_RADIO->EVENTS_PAYLOAD   = 0;

        NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;

        NRF_PPI->CHENCLR = ( 1 << compare0_rxen_ppi_channel );
        NRF_PPI->CHENSET =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        NRF_RADIO->INTENSET    = RADIO_INTENSET_DISABLED_Msk | RADIO_INTENSET_PAYLOAD_Msk;

        const std::uint32_t read_timeout = ( send_size + 1 + 4 + 3 ) * 8 + adv_reponse_timeout_us;
        state_ = state::adv_transmitting;

        if ( when.zero() )
        {
            NRF_RADIO->TASKS_TXEN          = 1;
            nrf_timer->TASKS_CAPTURE[ 1 ]  = 1;
            nrf_timer->CC[ 1 ]            += read_timeout + us_radio_tx_startup_time;
        }
        else
        {
            nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
            nrf_timer->CC[ 0 ]             = when.usec() - us_radio_tx_startup_time + anchor_offset_.usec();
            nrf_timer->CC[ 1 ]             = nrf_timer->CC[ 0 ] + us_radio_tx_startup_time + read_timeout;
            nrf_timer->CC[ 2 ]             = 0;

            // manually triggering event for timer beeing already behind target time
            nrf_timer->TASKS_CAPTURE[ 3 ]  = 1;

            // TODO: If timer wrapps, >= will fail!!!
            if ( nrf_timer->EVENTS_COMPARE[ 0 ] || nrf_timer->CC[ 3 ] >= nrf_timer->CC[ 0 ] )
            {
                state_ = state::adv_transmitting;
                nrf_timer->TASKS_CLEAR = 1;
                NRF_RADIO->TASKS_TXEN = 1;
            }
        }
    }


    void scheduled_radio_base::adv_radio_interrupt()
    {
        if ( NRF_RADIO->EVENTS_PAYLOAD )
        {
            NRF_RADIO->EVENTS_PAYLOAD = 0;

            if ( state_ == state::adv_transmitting )
            {
                NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer_.buffer );
                NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer_.size << RADIO_PCNF1_MAXLEN_Pos );

                NRF_RADIO->INTENCLR    = RADIO_INTENSET_PAYLOAD_Msk;
            }
        }

        if ( NRF_RADIO->EVENTS_DISABLED )
        {
            NRF_RADIO->EVENTS_DISABLED = 0;

            if ( state_ == state::adv_transmitting )
            {
                // stop the radio from receiving again
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
                state_ = state::adv_receiving;
            }
            else if ( state_ == state::adv_receiving )
            {
                // either we realy received something, or the timer disabled the radio.
                state_ = state::idle;

                // the anchor is the end of the connect request. The timer was captured with the radio end event
                anchor_offset_ = link_layer::delta_time( nrf_timer->CC[ 2 ] );

                if ( ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) == RADIO_CRCSTATUS_CRCSTATUS_CRCOk || receive_buffer_.buffer[ 1 ] != 0 )
                {
                    received_  = true;
                }
                else
                {
                    timeout_ = true;
                }
            }
        }
    }

    void scheduled_radio_base::adv_timer_interrupt()
    {
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

        state_                  = state::evt_wait_connect;
        receive_buffer_         = receive_buffer.empty()
            ? link_layer::read_buffer{ &empty_receive_[ 0 ], sizeof( empty_receive_ ) }
            : receive_buffer;

        NRF_RADIO->FREQUENCY   = frequency_from_channel( channel );
        NRF_RADIO->DATAWHITEIV = channel & 0x3F;
        NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer.buffer );
        NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer.size << RADIO_PCNF1_MAXLEN_Pos );

        NRF_RADIO->INTENCLR    = 0xffffffff;
        nrf_timer->INTENCLR    = 0xffffffff;

        NRF_RADIO->EVENTS_END       = 0;
        NRF_RADIO->EVENTS_DISABLED  = 0;
        NRF_RADIO->EVENTS_READY     = 0;
        NRF_RADIO->EVENTS_ADDRESS   = 0;

        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 1 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 2 ] = 0;

        // the hardware is wired to:
        // - start the receiving part of the radio, when the timer is equal to CC[ 0 ] (compare0_rxen_ppi_channel)
        // - when the radio ramped up for receiving, the receiving starts              (RADIO_SHORTS_READY_START_Msk)
        // - when the PDU was receieved, the timer value is captured in CC[ 2 ]        (radio_address_capture1_ppi_channel)
        // - when a PDU is received, the radio is stopped                              (RADIO_SHORTS_END_DISABLE_Msk)
        // - if no PDU is received, and the timer reaches CC[ 1 ], the radio is stopped(compare1_disable_ppi_channel)
        NRF_RADIO->SHORTS      = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

        NRF_PPI->CHENCLR =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare0_rxen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );
        NRF_PPI->CHENSET       =
              ( 1 << compare0_rxen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        NRF_RADIO->INTENSET    = RADIO_INTENSET_DISABLED_Msk;

        nrf_timer->CC[ 0 ] = start_receive.usec() + anchor_offset_.usec() - us_radio_rx_startup_time;
        nrf_timer->CC[ 1 ] = end_receive.usec() + anchor_offset_.usec() + 1000; // TODO: 1000: must depend on transmit size.
    }

    void scheduled_radio_base::evt_radio_interrupt()
    {
        if ( NRF_RADIO->EVENTS_DISABLED )
        {
            NRF_RADIO->EVENTS_DISABLED = 0;

            if ( state_ == state::evt_wait_connect )
            {
                // no need to disable the radio via the timer anymore:
                NRF_PPI->CHENCLR = ( 1 << radio_end_capture2_ppi_channel ) | ( 1 << compare1_disable_ppi_channel );

                const bool timeout   = nrf_timer->EVENTS_COMPARE[ 1 ];
                const bool crc_error = !timeout && ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) != RADIO_CRCSTATUS_CRCSTATUS_CRCOk;
                const bool error     = timeout || crc_error;

                if ( !error )
                {
                    NRF_RADIO->TASKS_TXEN  = 1;
                    const auto trans = receive_buffer_.buffer == &empty_receive_[ 0 ]
                        ? callbacks_.next_transmit()
                        : callbacks_.received_data( receive_buffer_ );

                    const_cast< std::uint8_t* >( trans.buffer )[ 0 ] = trans.buffer[ 0 ] & ~more_data_flag;

                    NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( trans.buffer );
                    NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( trans.size << RADIO_PCNF1_MAXLEN_Pos );

                    state_   = state::evt_transmiting_closing;

                    // the timer was captured with the end event; the anchor is the start of the receiving.
                    // Additional to the ll PDU length there are 1 byte preamble, 4 byte access address, 2 byte LL header and 3 byte crc
                    static constexpr std::size_t ll_pdu_overhead = 1 + 4 + 2 + 3;
                    const std::size_t total_pdu_length = receive_buffer_.buffer[ 1 ] + ll_pdu_overhead;
                    anchor_offset_ = link_layer::delta_time( nrf_timer->CC[ 2 ] - total_pdu_length * 8 );
                }
                else
                {
                    state_       = state::idle;
                    evt_timeout_ = true;
                }

                if ( timeout )
                    toggle_debug_pin1();

                if ( crc_error )
                    toggle_debug_pin2();

            }
            else if ( state_ == state::evt_transmiting_closing )
            {
                state_   = state::idle;
                end_evt_ = true;
            }
            else
            {
                assert( !"unrecognized radio state!" );
            }
        }
        else
        {
            assert( !"Unexpected Event source!" );
        }
    }

    void scheduled_radio_base::evt_timer_interrupt()
    {
    }

    void scheduled_radio_base::radio_interrupt()
    {
        if ( static_cast< unsigned >( state_ ) >= connection_event_type_base )
        {
            evt_radio_interrupt();
        }
        else
        {
            adv_radio_interrupt();
        }

    }

    void scheduled_radio_base::timer_interrupt()
    {
        if ( static_cast< unsigned >( state_ ) >= connection_event_type_base )
        {
            evt_timer_interrupt();
        }
        else
        {
            adv_timer_interrupt();
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
        while ( !received_ && !timeout_ && !evt_timeout_ && !end_evt_ && wake_up_ == 0 )
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

        if ( timeout_ )
        {
            timeout_ = false;
            callbacks_.adv_timeout();
        }

        if ( evt_timeout_ )
        {
            evt_timeout_ = false;
            callbacks_.timeout();
        }

        if ( end_evt_ )
        {
            end_evt_ = false;
            callbacks_.end_event();
        }

        if ( wake_up_ )
        {
            --wake_up_;
        }
    }

    void scheduled_radio_base::wake_up()
    {
        ++wake_up_;
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