#include <bluetoe/nrf51.hpp>

#include <nrf.h>
#include <core_cmInstr.h>

#include <cassert>
#include <cstdint>
#include <algorithm>
#include <cstring>

/*
 * Compile this with BLUETOE_NRF51_RADIO_DEBUG defined to enable debugging
 */

namespace bluetoe {
namespace nrf51_details {

    static NRF_RADIO_Type* const        nrf_radio            = NRF_RADIO;
    static NRF_TIMER_Type* const        nrf_timer            = NRF_TIMER0;
    static NRF_CCM_Type* const          nrf_ccm              = NRF_CCM;
    static NVIC_Type* const             nvic                 = NVIC;
    static NRF_PPI_Type* const          nrf_ppi              = NRF_PPI;
    static scheduled_radio_base*        instance             = nullptr;

    // after T_IFS (150µs +- 2) at maximum, a connection request will be received (34 Bytes + 1 Byte preable, 4 Bytes Access Address and 3 Bytes CRC)
    // plus some additional 20µs
    static constexpr std::uint32_t      adv_reponse_timeout_us   = 152 + 42 * 8 + 20;
    static constexpr std::uint8_t       maximum_advertising_pdu_size = 0x3f;

    static constexpr std::size_t        radio_ready_ccm_ksgen           = 24;
    static constexpr std::size_t        radio_address_ccm_crypt         = 25;
    static constexpr std::size_t        radio_address_capture1_ppi_channel = 26;
    static constexpr std::size_t        radio_end_capture2_ppi_channel  = 27;
    static constexpr std::size_t        compare0_txen_ppi_channel       = 20;
    static constexpr std::size_t        compare0_rxen_ppi_channel       = 21;
    static constexpr std::size_t        compare1_disable_ppi_channel    = 22;

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

    counter::counter()
        : low( 0 )
        , high( 0 )
    {
    }

    void counter::increment()
    {
        ++low;

        if ( low == 0 )
            ++high;
    }

    void counter::copy_to( std::uint8_t* target ) const
    {
        target  = details::write_32bit( target, low );
        *target = high;
    }

    /*
     * Frequency correction if NRF_FICR_Type has an OVERRIDEEN field
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

    static void init_radio( bool encryption_possible )
    {
        override_correction< NRF_FICR_Type >( NRF_FICR, NRF_RADIO );

        NRF_RADIO->MODE  = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;

        NRF_RADIO->PCNF0 =
            ( 1 << RADIO_PCNF0_S0LEN_Pos ) |
            ( 8 << RADIO_PCNF0_LFLEN_Pos ) |
            ( 0 << RADIO_PCNF0_S1LEN_Pos ) |
            ( encryption_possible
                ? ( RADIO_PCNF0_S1INCL_Include << RADIO_PCNF0_S1INCL_Pos )
                : ( RADIO_PCNF0_S1INCL_Automatic << RADIO_PCNF0_S1INCL_Pos ) );

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

    static int pdu_gap_required_by_encryption()
    {
        return NRF_RADIO->PCNF0 & ( RADIO_PCNF0_S1INCL_Include << RADIO_PCNF0_S1INCL_Pos )
            ? 1
            : 0;
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

    scheduled_radio_base::scheduled_radio_base( adv_callbacks& cbs, std::uint32_t encrypted_area )
        : callbacks_( cbs )
        , timeout_( false )
        , received_( false )
        , evt_timeout_( false )
        , end_evt_( false )
        , wake_up_( 0 )
        , state_( state::idle )
        , receive_encrypted_( false )
        , transmit_encrypted_( false )
        , encrypted_area_( encrypted_area )
    {
        // start high freuquence clock source if not done yet
        if ( !NRF_CLOCK->EVENTS_HFCLKSTARTED )
        {
            NRF_CLOCK->TASKS_HFCLKSTART = 1;

            while ( !NRF_CLOCK->EVENTS_HFCLKSTARTED )
                ;
        }

        init_debug();
        init_radio( encrypted_area_ != 0 );
        init_timer();

        instance = this;

        NVIC_SetPriority( RADIO_IRQn, 0 );
        NVIC_ClearPendingIRQ( RADIO_IRQn );
        NVIC_EnableIRQ( RADIO_IRQn );
        NVIC_ClearPendingIRQ( TIMER0_IRQn );
        NVIC_EnableIRQ( TIMER0_IRQn );
    }

    scheduled_radio_base::scheduled_radio_base( adv_callbacks& cbs )
        : scheduled_radio_base( cbs, false )
    {
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

        const int pdu_gap = pdu_gap_required_by_encryption();

        if ( !std::equal( &receive_buffer_.buffer[ pdu_header_size + addr_size + pdu_gap ], &receive_buffer_.buffer[ pdu_header_size + 2 * addr_size + pdu_gap ],
            &response_data_.buffer[ pdu_header_size + pdu_gap ] ) )
            return false;

        // in the scan request, the randomness is stored in RxAdd, in the scan response, it's stored in
        // TxAdd.
        const bool scanner_addres_is_random = response_data_.buffer[ 0 ] & tx_add_mask;
        if ( !static_cast< bool >( receive_buffer_.buffer[ 0 ] & rx_add_mask ) == scanner_addres_is_random )
            return false;

        const link_layer::device_address scanner( &receive_buffer_.buffer[ pdu_header_size + pdu_gap ], scanner_addres_is_random );

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

    void scheduled_radio_base::configure_encryption( bool receive, bool transmit )
    {
        receive_encrypted_ = receive;
        transmit_encrypted_ = transmit;
    }

    link_layer::delta_time scheduled_radio_base::start_connection_event_impl(
        unsigned                        channel,
        bluetoe::link_layer::delta_time start_receive,
        bluetoe::link_layer::delta_time end_receive,
        const link_layer::read_buffer&  receive_buffer )
    {
        while ((NRF_RADIO->STATE & RADIO_STATE_STATE_Msk) != RADIO_STATE_STATE_Disabled)
            ;

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
        NRF_RADIO->PCNF1       = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer_.size << RADIO_PCNF1_MAXLEN_Pos );

        if ( receive_encrypted_ )
        {
            nrf_ccm->EVENTS_ENDKSGEN = 0;
            nrf_ccm->EVENTS_ENDCRYPT = 0;
            nrf_ccm->EVENTS_ERROR    = 0;

            nrf_radio->PACKETPTR = encrypted_area_;
            nrf_ccm->OUTPTR      = reinterpret_cast< std::uint32_t >( receive_buffer_.buffer );
            nrf_ccm->INPTR       = encrypted_area_;

            nrf_ccm->MODE   =
                  ( CCM_MODE_MODE_Decryption << CCM_MODE_MODE_Pos )
                | ( CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos );
        }
        else
        {
            nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer_.buffer );
        }

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
        nrf_radio->SHORTS      = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;
        nrf_ccm->SHORTS        = 0;

        NRF_PPI->CHENCLR =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare0_rxen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel )
            | ( 1 << radio_ready_ccm_ksgen )
            | ( 1 << radio_address_ccm_crypt );

        NRF_PPI->CHENSET       =
              ( 1 << compare0_rxen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        if ( receive_encrypted_ )
            NRF_PPI->CHENSET = ( 1 << radio_ready_ccm_ksgen ) | ( 1 << radio_address_ccm_crypt );

        nrf_radio->INTENSET    = RADIO_INTENSET_DISABLED_Msk;

        nrf_timer->CC[ 0 ] = start_receive.usec() + anchor_offset_.usec() - us_radio_rx_startup_time;
        nrf_timer->CC[ 1 ] = end_receive.usec() + anchor_offset_.usec() + 1000; // TODO: 1000: must depend on transmit size.

        nrf_timer->TASKS_CAPTURE[ 3 ] = 1;

        return link_layer::delta_time::usec( nrf_timer->CC[ 0 ] - nrf_timer->CC[ 3 ] );
    }

    void scheduled_radio_base::evt_radio_interrupt()
    {
        if ( nrf_radio->EVENTS_DISABLED )
        {
            nrf_radio->EVENTS_DISABLED = 0;

            if ( state_ == state::evt_wait_connect )
            {
                // no need to disable the radio via the timer anymore:
                NRF_PPI->CHENCLR = ( 1 << radio_end_capture2_ppi_channel ) | ( 1 << compare1_disable_ppi_channel );
                // Transmission has been startet already, make sure, radio gets disabled
                nrf_radio->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

                const bool timeout   = nrf_timer->EVENTS_COMPARE[ 1 ];
                const bool crc_error = !timeout && ( nrf_radio->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) != RADIO_CRCSTATUS_CRCSTATUS_CRCOk;
                const bool mic_error = receive_encrypted_ && receive_buffer_.buffer[ 1 ] != 0 && ( nrf_ccm->MICSTATUS & CCM_MICSTATUS_MICSTATUS_Msk ) == CCM_MICSTATUS_MICSTATUS_CheckFailed;
                const bool bus_error = receive_encrypted_ && nrf_ccm->EVENTS_ERROR;
                const bool error     = timeout || crc_error || mic_error || bus_error;

                if ( !error )
                {
                    // switch to transmission
                    const auto trans = receive_buffer_.buffer == &empty_receive_[ 0 ]
                        ? callbacks_.next_transmit()
                        : callbacks_.received_data( receive_buffer_ );

                    const_cast< std::uint8_t* >( trans.buffer )[ 0 ] = trans.buffer[ 0 ] & ~more_data_flag;

                    if ( transmit_encrypted_ )
                    {
                        nrf_radio->PACKETPTR = encrypted_area_;

                        nrf_ccm->SHORTS  = CCM_SHORTS_ENDKSGEN_CRYPT_Msk;
                        nrf_ccm->MODE   =
                              ( CCM_MODE_MODE_Encryption << CCM_MODE_MODE_Pos )
                            | ( CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos );
                        nrf_ccm->OUTPTR = encrypted_area_;
                        nrf_ccm->INPTR  = reinterpret_cast< std::uint32_t >( trans.buffer );

                        NRF_PPI->CHENCLR = ( 1 << radio_address_ccm_crypt );
                    }
                    else
                    {
                        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( trans.buffer );
                    }

                    nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( trans.size << RADIO_PCNF1_MAXLEN_Pos );

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
                    nrf_ccm->EVENTS_ERROR    = 0;
                    NRF_RADIO->SHORTS        = 0;
                    NRF_RADIO->TASKS_DISABLE = 1;
                    state_       = state::idle;
                    evt_timeout_ = true;

                    if ( timeout )
                        debug_timeout();

                    if ( crc_error )
                        debug_crc_error();
                }
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

            receive_buffer_.size = std::min< std::size_t >(
                receive_buffer_.size,
                ( receive_buffer_.buffer[ 1 ] & 0x3f ) + 2 + pdu_gap_required_by_encryption()
            );
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

    /*
     * With encryption
     */

    // simply for easier debugging
    static NRF_RNG_Type* const          nrf_random  = NRF_RNG;
    static NRF_ECB_Type* const          nrf_aes     = NRF_ECB;

    static std::uint8_t random_number8()
    {
        nrf_random->TASKS_START = 1;

        while ( !nrf_random->EVENTS_VALRDY )
            ;

        nrf_random->EVENTS_VALRDY = 0;

        return nrf_random->VALUE;
    }

    static std::uint16_t random_number16()
    {
        return static_cast< std::uint16_t >( random_number8() )
            | ( static_cast< std::uint16_t >( random_number8() ) << 8 );
    }

    static std::uint32_t random_number32()
    {
        return static_cast< std::uint32_t >( random_number16() )
            | ( static_cast< std::uint32_t >( random_number16() ) << 16 );
    }

    static std::uint64_t random_number64()
    {
        return static_cast< std::uint64_t >( random_number32() )
            | ( static_cast< std::uint64_t >( random_number32() ) << 32 );
    }

    static struct alignas( 4 ) ccm_data_struct_t {
        std::uint8_t data[ 33 ];
    } ccm_data_struct;

    static constexpr std::size_t ccm_key_offset = 0;
    static constexpr std::size_t ccm_packet_counter_offset = 16;
    static constexpr std::size_t ccm_packet_counter_size   = 5;
    static constexpr std::size_t ccm_direction_offset = 24;
    static constexpr std::size_t ccm_iv_offset = 25;

    static constexpr std::uint8_t master_to_slave_ccm_direction = 0x01;
    static constexpr std::uint8_t slave_to_master_ccm_direction = 0x00;

    static void init_ccm_data_structure( std::uint32_t scratch_area )
    {
        nrf_ccm->SCRATCHPTR = scratch_area;
        nrf_ccm->CNFPTR     = reinterpret_cast< std::uintptr_t >( &ccm_data_struct );
    }

    static void setup_ccm_data_structure( const bluetoe::details::uint128_t& key, std::uint64_t IV )
    {
        std::copy( key.begin(), key.end(), &ccm_data_struct.data[ ccm_key_offset ] );
        std::fill( &ccm_data_struct.data[ ccm_packet_counter_offset ],
            &ccm_data_struct.data[ ccm_packet_counter_offset + ccm_packet_counter_size ], 0 );

        details::write_64bit( &ccm_data_struct.data[ ccm_iv_offset ], IV );
    }

    static bluetoe::details::uint128_t aes( const bluetoe::details::uint128_t& key, const bluetoe::details::uint128_t& data )
    {
        static struct alignas( 4 ) ecb_data_t {
            std::uint8_t data[ 3 * 16 ];
        } ecb_scratch_data;

        nrf_aes->ECBDATAPTR = reinterpret_cast< std::uint32_t >( &ecb_scratch_data.data[ 0 ] );

        std::copy( key.rbegin(), key.rend(), &ecb_scratch_data.data[ 0 ] );
        std::copy( data.rbegin(), data.rend(), &ecb_scratch_data.data[ 16 ] );

        nrf_aes->TASKS_STARTECB = 1;

        while ( !nrf_aes->EVENTS_ENDECB && !nrf_aes->EVENTS_ERRORECB )
            ;

        assert( !nrf_aes->EVENTS_ERRORECB );
        nrf_aes->EVENTS_ENDECB = 0;

        bluetoe::details::uint128_t result;
        std::copy( &ecb_scratch_data.data[ 32 ], &ecb_scratch_data.data[ 48 ], result.rbegin() );

        // erase key out of memory
        std::fill( &ecb_scratch_data.data[ 0 ], &ecb_scratch_data.data[ 16 ], 0 );

        return result;
    }

    static bluetoe::details::uint128_t xor_( bluetoe::details::uint128_t a, const bluetoe::details::uint128_t& b )
    {
        std::transform(
            a.begin(), a.end(),
            b.begin(),
            a.begin(),
            []( std::uint8_t a, std::uint8_t b ) -> std::uint8_t
            {
                return a xor b;
            }
        );

        return a;
    }

    scheduled_radio_base_with_encryption_base::scheduled_radio_base_with_encryption_base(
        adv_callbacks& cbs, std::uint32_t scratch_area, std::uint32_t encrypted_area )
        : scheduled_radio_base( cbs, encrypted_area )
    {
        nrf_random->CONFIG = RNG_CONFIG_DERCEN_Msk;
        nrf_random->SHORTS = RNG_SHORTS_VALRDY_STOP_Msk;

        init_ccm_data_structure( scratch_area );
    }

    details::uint128_t scheduled_radio_base_with_encryption_base::create_srand()
    {
        details::uint128_t result;
        std::generate( result.begin(), result.end(), random_number8 );

        return result;
    }

    details::longterm_key_t scheduled_radio_base_with_encryption_base::create_long_term_key()
    {
        const details::longterm_key_t result = {
            create_srand(),
            random_number64(),
            random_number16()
        };

        return result;
    }

    details::uint128_t scheduled_radio_base_with_encryption_base::c1(
        const bluetoe::details::uint128_t& temp_key,
        const bluetoe::details::uint128_t& rand,
        const bluetoe::details::uint128_t& p1,
        const bluetoe::details::uint128_t& p2 ) const
    {
        // c1 (k, r, preq, pres, iat, rat, ia, ra) = e(k, e(k, r XOR p1) XOR p2)
        const auto p1_ = aes( temp_key, xor_( rand, p1 ) );

        return aes( temp_key, xor_( p1_, p2 ) );
    }

    bluetoe::details::uint128_t scheduled_radio_base_with_encryption_base::s1(
        const bluetoe::details::uint128_t& temp_key,
        const bluetoe::details::uint128_t& srand,
        const bluetoe::details::uint128_t& mrand )
    {
        bluetoe::details::uint128_t r;
        std::copy( &srand[ 0 ], &srand[ 8 ], &r[ 8 ] );
        std::copy( &mrand[ 0 ], &mrand[ 8 ], &r[ 0 ] );

        return aes( temp_key, r );
    }

    std::pair< std::uint64_t, std::uint32_t > scheduled_radio_base_with_encryption_base::setup_encryption(
        bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm )
    {
        const std::uint64_t skds = random_number64();
        const std::uint32_t ivs  = random_number32();

        bluetoe::details::uint128_t session_descriminator;
        details::write_64bit( &session_descriminator[ 0 ], skdm );
        details::write_64bit( &session_descriminator[ 8 ], skds );

        setup_ccm_data_structure(
            aes( key, session_descriminator),
            static_cast< std::uint64_t >( ivm ) | ( static_cast< std::uint64_t >( ivs ) << 32 ) );

        return { skds, ivs };
    }

    void scheduled_radio_base_with_encryption_base::start_receive_encrypted()
    {
        rx_counter_ = counter();
        nrf_ccm->ENABLE         = CCM_ENABLE_ENABLE_Enabled << CCM_ENABLE_ENABLE_Pos;
        configure_encryption( true, false );
    }

    void scheduled_radio_base_with_encryption_base::start_transmit_encrypted()
    {
        tx_counter_ = counter();
        configure_encryption( true, true );
    }

    void scheduled_radio_base_with_encryption_base::stop_receive_encrypted()
    {
        configure_encryption( false, true );
    }

    void scheduled_radio_base_with_encryption_base::stop_transmit_encrypted()
    {
        configure_encryption( false, false );

        // remove key from memory
        std::memset( &ccm_data_struct, 0, sizeof( ccm_data_struct ) );
        nrf_ccm->ENABLE = nrf_ccm->ENABLE & ~CCM_ENABLE_ENABLE_Msk;
    }

    void scheduled_radio_base_with_encryption_base::load_receive_packet_counter()
    {
        rx_counter_.copy_to( &ccm_data_struct.data[ ccm_packet_counter_offset ] );
        ccm_data_struct.data[ ccm_direction_offset ] = master_to_slave_ccm_direction;
    }

    void scheduled_radio_base_with_encryption_base::load_transmit_packet_counter()
    {
        tx_counter_.copy_to( &ccm_data_struct.data[ ccm_packet_counter_offset ] );
        ccm_data_struct.data[ ccm_direction_offset ] = slave_to_master_ccm_direction;
    }

} // namespace nrf51_details
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
