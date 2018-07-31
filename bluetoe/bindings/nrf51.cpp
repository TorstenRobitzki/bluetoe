#include <bluetoe/bindings/nrf51.hpp>

#include <nrf.h>
#include <core_cmInstr.h>

#include <cassert>
#include <cstdint>
#include <algorithm>

/*
 * Compile this with BLUETOE_NRF51_RADIO_DEBUG defined to enable debugging
 */

namespace bluetoe {
namespace nrf51_details {

    static NRF_RADIO_Type* const        nrf_radio            = NRF_RADIO;
    static NRF_TIMER_Type* const        nrf_timer            = NRF_TIMER0;
    static NVIC_Type* const             nvic                 = NVIC;
    static NRF_PPI_Type* const          nrf_ppi              = NRF_PPI;
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

#   if defined BLUETOE_NRF51_RADIO_DEBUG
        static constexpr int debug_pin_time_nr      = 17;
        static constexpr int debug_pin_crc_error_nr = 18;
        static constexpr int debug_pin_irs_nr       = 19;
        static constexpr int debug_pin_cs_nr        = 20;
        static constexpr int debug_pin_timer_irs_nr = 22;
        static constexpr int debug_pin_transmit     = 23;
        static constexpr int debug_pin_receive      = 24;
        static constexpr int debug_pin_adv_response = 25;

        void init_debug()
        {
            for ( auto pin : { debug_pin_time_nr, debug_pin_crc_error_nr, debug_pin_irs_nr,
                debug_pin_cs_nr, debug_pin_timer_irs_nr, debug_pin_transmit, debug_pin_receive,
                debug_pin_adv_response } )
            {
                NRF_GPIO->PIN_CNF[ pin ] =
                    ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
                    ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );
            }

            NRF_GPIOTE->CONFIG[ 0 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_transmit << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_PPI->CH[ 0 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_ADDRESS );
            NRF_PPI->CH[ 0 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 0 ] );

            NRF_PPI->CH[ 1 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_END );
            NRF_PPI->CH[ 1 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 0 ] );

            NRF_PPI->CHENSET = 0x3;
        }

        void debug_end_radio()
        {
            NRF_GPIOTE->TASKS_CLR[0] = 1;
        }

        void pulse_debug_pin( int pin )
        {
            NRF_GPIO->OUTSET = 1 << pin;
            NRF_GPIO->OUTCLR = 1 << pin;
        }

        void debug_timeout()
        {
            pulse_debug_pin( debug_pin_time_nr );
        }

        void debug_crc_error()
        {
            pulse_debug_pin( debug_pin_crc_error_nr );
        }

        void debug_enter_isr()
        {
            NRF_GPIO->OUTSET = 1 << debug_pin_irs_nr;
        }

        void debug_leave_isr()
        {
            NRF_GPIO->OUTCLR = 1 << debug_pin_irs_nr;
        }

        void debug_enter_timer_isr()
        {
            NRF_GPIO->OUTSET = 1 << debug_pin_timer_irs_nr;
        }

        void debug_leave_timer_isr()
        {
            NRF_GPIO->OUTCLR = 1 << debug_pin_timer_irs_nr;
        }

        void debug_enter_critical_section()
        {
            NRF_GPIO->OUTSET = 1 << debug_pin_cs_nr;
        }

        void debug_leave_critical_section()
        {
            NRF_GPIO->OUTCLR = 1 << debug_pin_cs_nr;
        }

        void debug_adv_response()
        {
            pulse_debug_pin( debug_pin_adv_response );
        }
#   else
        void init_debug() {}
        void debug_end_radio() {}
        void debug_timeout() {}
        void debug_crc_error() {}
        void debug_enter_isr() {}
        void debug_leave_isr() {}
        void debug_enter_timer_isr() {}
        void debug_leave_timer_isr() {}
        void debug_enter_critical_section() {}
        void debug_leave_critical_section() {}
        void debug_adv_response() {}
#   endif

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

        // TIFS is only enforced if END_DISABLE and DISABLED_TXEN shortcuts are enabled.
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
        debug_enter_critical_section();

        __disable_irq();
    }

    scheduled_radio_base::lock_guard::~lock_guard()
    {
        __set_PRIMASK( context_ );

        debug_leave_critical_section();
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

        init_debug();
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

    void scheduled_radio_base::schedule_advertisment(
            unsigned                        channel,
            const link_layer::write_buffer& advertising_data,
            const link_layer::write_buffer& response_data,
            link_layer::delta_time          when,
            const link_layer::read_buffer&  receive )
    {
        while ((NRF_RADIO->STATE & RADIO_STATE_STATE_Msk) != RADIO_STATE_STATE_Disabled);
        assert( ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) == RADIO_STATE_STATE_Disabled );
        assert( !received_ );
        assert( !timeout_ );
        assert( state_ == state::idle );
        assert( receive.buffer && receive.size >= 2u );
        assert( response_data.buffer );

        const std::uint8_t  send_size  = std::min< std::size_t >( advertising_data.size, maximum_advertising_pdu_size );

        response_data_       = response_data;
        receive_buffer_      = receive;
        receive_buffer_.size = std::min< std::size_t >( receive.size, maximum_advertising_pdu_size );


        NRF_RADIO->FREQUENCY   = frequency_from_channel( channel );
        NRF_RADIO->DATAWHITEIV = channel & 0x3F;

        NRF_RADIO->SHORTS      = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;
        NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( advertising_data.buffer );
        NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( send_size << RADIO_PCNF1_MAXLEN_Pos );

        NRF_RADIO->INTENCLR    = 0xffffffff;
        nrf_timer->INTENCLR    = 0xffffffff;

        NRF_RADIO->EVENTS_END       = 0;
        NRF_RADIO->EVENTS_DISABLED  = 0;
        NRF_RADIO->EVENTS_READY     = 0;
        NRF_RADIO->EVENTS_ADDRESS   = 0;
        NRF_RADIO->EVENTS_PAYLOAD   = 0;

        NRF_PPI->CHENCLR = ( 1 << compare0_rxen_ppi_channel );
        NRF_PPI->CHENSET =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        NRF_RADIO->INTENSET    = RADIO_INTENSET_DISABLED_Msk;

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
                nrf_timer->TASKS_CLEAR = 1;
                NRF_RADIO->TASKS_TXEN = 1;
            }
        }
    }

    bool scheduled_radio_base::is_valid_scan_request() const
    {
        static constexpr std::uint8_t scan_request_size     = 12;
        static constexpr std::uint8_t scan_request_pdu_type = 0x03;
        static constexpr std::uint8_t pdu_type_mask         = 0x0F;
        static constexpr int          pdu_header_size       = 2;
        static constexpr int          addr_size             = 6;
        static constexpr std::uint8_t tx_add_mask           = 0x40;
        static constexpr std::uint8_t rx_add_mask           = 0x80;

        if ( receive_buffer_.buffer[ 1 ] != scan_request_size )
            return false;

        if ( ( receive_buffer_.buffer[ 0 ] & pdu_type_mask ) != scan_request_pdu_type )
            return false;

        if ( !std::equal( &receive_buffer_.buffer[ pdu_header_size + addr_size ], &receive_buffer_.buffer[ pdu_header_size + 2 * addr_size ],
            &response_data_.buffer[ pdu_header_size ] ) )
            return false;

        // in the scan request, the randomness is stored in RxAdd, in the scan response, it's stored in
        // TxAdd.
        const bool scanner_addres_is_random = response_data_.buffer[ 0 ] & tx_add_mask;
        if ( !static_cast< bool >( receive_buffer_.buffer[ 0 ] & rx_add_mask ) == scanner_addres_is_random )
            return false;

        const link_layer::device_address scanner( &receive_buffer_.buffer[ pdu_header_size ], scanner_addres_is_random );

        return callbacks_.is_scan_request_in_filter_callback( scanner );
    }

    void scheduled_radio_base::stop_radio()
    {
        NRF_RADIO->SHORTS        = 0;
        NRF_RADIO->TASKS_DISABLE = 1;

        state_      = state::idle;
    }

    void scheduled_radio_base::adv_radio_interrupt()
    {
        if ( NRF_RADIO->EVENTS_DISABLED )
        {
            NRF_RADIO->EVENTS_DISABLED = 0;

            if ( state_ == state::adv_transmitting )
            {
                NRF_RADIO->SHORTS      = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;

                NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer_.buffer );
                NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer_.size << RADIO_PCNF1_MAXLEN_Pos );

                state_ = state::adv_receiving;
            }
            else if ( state_ == state::adv_receiving )
            {
                // the anchor is the end of the connect request. The timer was captured with the radio end event
                anchor_offset_ = link_layer::delta_time( nrf_timer->CC[ 2 ] );

                // either we realy received something, or the timer disabled the radio.
                if ( ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) == RADIO_CRCSTATUS_CRCSTATUS_CRCOk && NRF_RADIO->EVENTS_PAYLOAD )
                {
                    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

                    // stop the timer from canceling the read!
                    NRF_PPI->CHENCLR  = ( 1 << compare0_txen_ppi_channel )
                                      | ( 1 << compare1_disable_ppi_channel )
                                      | ( 1 << radio_end_capture2_ppi_channel );

                    if ( is_valid_scan_request() )
                    {
                        state_ = state::adv_transmitting_response;

                        NRF_RADIO->PACKETPTR   = reinterpret_cast< std::uint32_t >( response_data_.buffer );
                        NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( response_data_.size << RADIO_PCNF1_MAXLEN_Pos );

                        debug_adv_response();
                    }
                    else
                    {
                        stop_radio();
                        received_   = true;
                    }
                }
                else
                {
                    stop_radio();
                    timeout_    = true;
                }
            }
            else if ( state_ == state::adv_transmitting_response )
            {
                stop_radio();
                timeout_    = true;
            }

            NRF_RADIO->EVENTS_PAYLOAD = 0;

            debug_end_radio();
        }
    }

    void scheduled_radio_base::adv_timer_interrupt()
    {
    }

    link_layer::delta_time scheduled_radio_base::start_connection_event(
        unsigned                        channel,
        bluetoe::link_layer::delta_time start_receive,
        bluetoe::link_layer::delta_time end_receive,
        const link_layer::read_buffer&  receive_buffer )
    {
        while ((NRF_RADIO->STATE & RADIO_STATE_STATE_Msk) != RADIO_STATE_STATE_Disabled);
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
        NRF_RADIO->SHORTS      = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;

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

        nrf_timer->TASKS_CAPTURE[ 3 ] = 1;

        return link_layer::delta_time::usec( nrf_timer->CC[ 0 ] - nrf_timer->CC[ 3 ] );
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
                // Transmission has been startet already, make sure, radio gets disabled
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

                const bool timeout   = nrf_timer->EVENTS_COMPARE[ 1 ];
                const bool crc_error = !timeout && ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) != RADIO_CRCSTATUS_CRCSTATUS_CRCOk;
                const bool error     = timeout || crc_error;

                if ( !error )
                {
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
                    debug_enter_timer_isr();
                    debug_leave_timer_isr();
                }
                else
                {
                    NRF_RADIO->SHORTS        = 0;
                    NRF_RADIO->TASKS_DISABLE = 1;
                    state_       = state::idle;
                    evt_timeout_ = true;
                }

                if ( timeout )
                    debug_timeout();

                if ( crc_error )
                    debug_crc_error();

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
        while ( !received_ && !timeout_ && !evt_timeout_ && !end_evt_ && wake_up_ == 0 )
            __WFI();

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
    bluetoe::nrf51_details::debug_enter_isr();

    bluetoe::nrf51_details::instance->radio_interrupt();

    bluetoe::nrf51_details::debug_leave_isr();
}

extern "C" void TIMER0_IRQHandler(void)
{
    bluetoe::nrf51_details::debug_enter_timer_isr();

    bluetoe::nrf51_details::instance->timer_interrupt();

    bluetoe::nrf51_details::debug_leave_timer_isr();
}