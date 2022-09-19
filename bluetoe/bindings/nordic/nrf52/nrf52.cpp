#include <bluetoe/nrf52.hpp>

#include <bluetoe/bits.hpp>

#include <cstring>
#include <algorithm>

namespace bluetoe
{
using namespace bluetoe::nrf;

namespace nrf52_details
{
    static constexpr std::size_t        radio_address_ccm_crypt         = 25;
    static constexpr std::size_t        radio_end_capture2_ppi_channel  = 27;
    static constexpr std::size_t        compare0_txen_ppi_channel       = 20;
    static constexpr std::size_t        compare0_rxen_ppi_channel       = 21;
    static constexpr std::size_t        compare1_disable_ppi_channel    = 22;
    static constexpr std::size_t        radio_bcmatch_aar_start_channel = 23;
    static constexpr std::size_t        rtc0_start_tim_ppi_channel      = 31;

    static constexpr std::uint32_t      all_preprogramed_ppi_channels_mask =
        ( 1 << radio_address_ccm_crypt )
      | ( 1 << radio_end_capture2_ppi_channel )
      | ( 1 << compare0_txen_ppi_channel )
      | ( 1 << compare0_rxen_ppi_channel )
      | ( 1 << compare1_disable_ppi_channel )
      | ( 1 << radio_bcmatch_aar_start_channel )
      | ( 1 << rtc0_start_tim_ppi_channel );

    static constexpr std::uint32_t      transmit_ppi_channels =
        ( 1 << compare0_txen_ppi_channel )
      | ( 1 << compare1_disable_ppi_channel );

    static constexpr std::uint32_t      receive_ppi_channels =
        ( 1 << compare0_rxen_ppi_channel )
      | ( 1 << compare1_disable_ppi_channel );

    // allocation of PPI channels
    enum ppi_channels_used {
        // Channels only used, if BLUETOE_NRF52_RADIO_DEBUG is set
        trace_debug_ppi_channel_1        = 9,
        trace_debug_ppi_channel_2        = 10,
        trace_clock_hfxo_ppi_channel     = 11,
        trace_radio_address_ppi_channel  = 12,
        trace_radio_end_ppi_channel      = 13,
        trace_ccm_endcrypt_ppi_channel   = 14,
        trace_ccm_endksgen_ppi_channel   = 15,
        trace_radio_ready_ppi_channel    = 16,
        trace_radio_disabled_ppi_channel = 17,
        // Channels always used
        // !!! If this allocation changes, make sure, the documentation in nrf52.hpp is updated
        rtc_start_cb_timer_ppi_channel   = 18,
        rtc_start_hfxo_ppi_channel       = 19,
    };

    static constexpr std::uint32_t      all_radio_ppi_channels_mask =
        all_preprogramed_ppi_channels_mask
      | ( 1 << rtc_start_hfxo_ppi_channel );

    // Allocation of the compare/capture registers of the RTC
    enum rtc_capture_registers {
        rtc_cc_start_timer = 0,
        rtc_cc_start_hfxo  = 1,
        rtc_cc_start_cb_timer = 2,
    };

    // Allocation of the compare/capture registers of TIMER0
    enum timer_capture_registers {
        tim_cc_start_radio    = 0,
        tim_cc_timeout        = 1,
        tim_cc_capture_anchor = 2,
        tim_cc_capture_now    = 3
    };

    // Allocation of the compare/capture registers of TIMER1
    enum cb_timer_capture_registers {
        cb_tim_cc_start_exec_cb = 0,
        cb_tim_cc_assert_end_cb = 1,
    };

    static void assign_channel(
        ppi_channels_used channel,
        volatile std::uint32_t& event,
        volatile std::uint32_t& task )
    {
       nrf_ppi->CH[ channel ].EEP = reinterpret_cast< std::uint32_t >( &event );
       nrf_ppi->CH[ channel ].TEP = reinterpret_cast< std::uint32_t >( &task );
    }

    static constexpr std::uint32_t timer_prescale_for_1us_resolution = 4;

#   if defined BLUETOE_NRF52_RADIO_DEBUG
        static constexpr int debug_pin_end_crypt     = 11;
        static constexpr int debug_pin_ready_disable = 12;
        static constexpr int debug_pin_address_end   = 13;
        static constexpr int debug_pin_keystream     = 14;
        static constexpr int debug_pin_debug         = 15;
        static constexpr int debug_pin_isr           = 16;
        static constexpr int debug_pin_hfxo          = 17;

        void init_debug()
        {
            for ( auto pin : { debug_pin_end_crypt, debug_pin_ready_disable,
                                debug_pin_address_end, debug_pin_keystream, debug_pin_debug,
                                debug_pin_isr, debug_pin_hfxo } )
            {
                NRF_GPIO->PIN_CNF[ pin ] =
                    ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
                    ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );
            }

            NRF_GPIOTE->CONFIG[ 0 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_address_end << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_GPIOTE->CONFIG[ 1 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_keystream << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_GPIOTE->CONFIG[ 2 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_ready_disable << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_GPIOTE->CONFIG[ 3 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_end_crypt << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_GPIOTE->CONFIG[ 4 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_hfxo << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_GPIOTE->CONFIG[ 5 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_debug << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            assign_channel( trace_clock_hfxo_ppi_channel, nrf_clock->EVENTS_HFCLKSTARTED, NRF_GPIOTE->TASKS_SET[ 4 ] );
            assign_channel( trace_radio_address_ppi_channel, NRF_RADIO->EVENTS_ADDRESS, NRF_GPIOTE->TASKS_SET[ 0 ] );
            assign_channel( trace_radio_end_ppi_channel, NRF_RADIO->EVENTS_END, NRF_GPIOTE->TASKS_CLR[ 0 ] );
            assign_channel( trace_ccm_endcrypt_ppi_channel, NRF_CCM->EVENTS_ENDCRYPT, NRF_GPIOTE->TASKS_OUT[ 3 ] );
            assign_channel( trace_ccm_endksgen_ppi_channel, NRF_CCM->EVENTS_ENDKSGEN, NRF_GPIOTE->TASKS_CLR[ 1 ] );
            assign_channel( trace_radio_ready_ppi_channel, NRF_RADIO->EVENTS_READY, NRF_GPIOTE->TASKS_OUT[ 2 ] );
            assign_channel( trace_radio_disabled_ppi_channel, NRF_RADIO->EVENTS_DISABLED, NRF_GPIOTE->TASKS_OUT[ 2 ] );
            assign_channel( trace_debug_ppi_channel_1, nrf_timer->EVENTS_COMPARE[ tim_cc_timeout ], NRF_GPIOTE->TASKS_OUT[ 1 ] );

            NRF_PPI->CHENSET =
                ( 1 << trace_debug_ppi_channel_1 )
              | ( 1 << trace_debug_ppi_channel_2 )
              | ( 1 << trace_clock_hfxo_ppi_channel )
              | ( 1 << trace_radio_address_ppi_channel )
              | ( 1 << trace_radio_end_ppi_channel )
              | ( 1 << trace_ccm_endcrypt_ppi_channel )
              | ( 1 << trace_ccm_endksgen_ppi_channel )
              | ( 1 << trace_radio_ready_ppi_channel )
              | ( 1 << trace_radio_disabled_ppi_channel );
        }

        void toggle_debug_pin()
        {
            NRF_GPIOTE->TASKS_OUT[ 5 ] = 1;
            NRF_GPIOTE->TASKS_OUT[ 5 ] = 1;
        }

        void set_isr_pin()
        {
            NRF_GPIO->OUTSET = ( 1 << debug_pin_isr );
        }

        void reset_isr_pin()
        {
            NRF_GPIO->OUTCLR = ( 1 << debug_pin_isr );
        }

        void gpio_debug_hfxo_stopped()
        {
            NRF_GPIOTE->TASKS_CLR[ 4 ] = 1;
        }

        struct record_long_distance_timer_params_t {
            std::uint32_t hf_anchor;
            std::uint32_t lf_anchor;
            std::uint32_t us_radio_start_time;
            std::uint32_t us_radio_startup_delay;
            std::uint32_t us_radio_timeout;
            std::uint32_t timer;
            std::uint32_t clock;
            std::uint32_t rtc_cc0;
            std::uint32_t rtc_cc1;
            std::uint32_t timer_cc0;
            std::uint32_t timer_cc1;
        };

        static constexpr std::size_t record_long_distance_timer_size = 16;
        record_long_distance_timer_params_t record_long_distance_timer_[ record_long_distance_timer_size ] = { { 0 } };
        int record_long_distance_timer_ptr_ = 0;

        void record_long_distance_timer(
            const std::uint32_t hf_anchor,
            const std::uint32_t lf_anchor,
            const std::uint32_t us_radio_start_time,
            const std::uint32_t us_radio_startup_delay,
            const std::uint32_t us_radio_timeout )
        {
            nrf_timer->TASKS_CAPTURE[ tim_cc_capture_now ] = 1;
            const std::uint32_t now = nrf_timer->CC[ tim_cc_capture_now ];

            record_long_distance_timer_[ record_long_distance_timer_ptr_ ] = record_long_distance_timer_params_t{
                .hf_anchor              = hf_anchor,
                .lf_anchor              = lf_anchor,
                .us_radio_start_time    = us_radio_start_time,
                .us_radio_startup_delay = us_radio_startup_delay,
                .us_radio_timeout       = us_radio_timeout,
                .timer                  = now,
                .clock                  = nrf_rtc->COUNTER,
                .rtc_cc0                = nrf_rtc->CC[ rtc_cc_start_timer ],
                .rtc_cc1                = nrf_rtc->CC[ rtc_cc_start_hfxo ],
                .timer_cc0              = nrf_timer->CC[ tim_cc_start_radio ],
                .timer_cc1              = nrf_timer->CC[ tim_cc_timeout ]
            };

            record_long_distance_timer_ptr_ = ( record_long_distance_timer_ptr_ + 1 ) % record_long_distance_timer_size;
        }
#   else
        void init_debug() {}

        void toggle_debug_pin() {}
        void set_isr_pin() {}
        void reset_isr_pin() {}
        void gpio_debug_hfxo_stopped() {}
        void record_long_distance_timer(
            const std::uint32_t,
            const std::uint32_t,
            const std::uint32_t,
            const std::uint32_t,
            const std::uint32_t ) {}
#   endif

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
        // TODO Use MODECNF0.RU to speedup rampup?
        override_correction< NRF_FICR_Type >( NRF_FICR, nrf_radio );

        nrf_radio->MODE  = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;

        nrf_radio->PCNF0 =
            ( 1 << RADIO_PCNF0_S0LEN_Pos ) |
            ( 8 << RADIO_PCNF0_LFLEN_Pos ) |
            ( 0 << RADIO_PCNF0_S1LEN_Pos ) |
            ( encryption_possible
                ? ( RADIO_PCNF0_S1INCL_Include << RADIO_PCNF0_S1INCL_Pos )
                : ( RADIO_PCNF0_S1INCL_Automatic << RADIO_PCNF0_S1INCL_Pos ) );

        nrf_radio->PCNF1 =
            ( RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos ) |
            ( RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos ) |
            ( 3 << RADIO_PCNF1_BALEN_Pos ) |
            ( 0 << RADIO_PCNF1_STATLEN_Pos );

        nrf_radio->TXADDRESS = 0;
        nrf_radio->RXADDRESSES = 1 << 0;

        nrf_radio->CRCCNF    =
            ( RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos ) |
            ( RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos );

        // clear all used PPI pre-programmed channels (16.1.1)
        nrf_ppi->CHENCLR = all_preprogramed_ppi_channels_mask;

        // The polynomial has the form of x^24 +x^10 +x^9 +x^6 +x^4 +x^3 +x+1
        nrf_radio->CRCPOLY   = 0x100065B;

        // TIFS is only enforced if END_DISABLE and DISABLED_TXEN shortcuts are enabled.
        nrf_radio->TIFS      = 150;
    }

    static void configure_timer_for_1us( NRF_TIMER_Type& timer )
    {
        timer.MODE        = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
        timer.BITMODE     = TIMER_BITMODE_BITMODE_32Bit;
        timer.PRESCALER   = timer_prescale_for_1us_resolution;

        timer.TASKS_STOP  = 1;
        timer.TASKS_CLEAR = 1;
        timer.EVENTS_COMPARE[ 0 ] = 0;
        timer.EVENTS_COMPARE[ 1 ] = 0;
        timer.EVENTS_COMPARE[ 2 ] = 0;
        timer.EVENTS_COMPARE[ 3 ] = 0;
    }

    static void init_timer()
    {
        configure_timer_for_1us( *nrf_timer );
        nrf_timer->TASKS_START = 1;
    }

    static void init_ppi()
    {
        assign_channel(
            rtc_start_hfxo_ppi_channel,
            nrf_rtc->EVENTS_COMPARE[ rtc_cc_start_hfxo ],
            nrf_clock->TASKS_HFCLKSTART );
    }

    static void* instance = nullptr;
    static void (*isr_handler)( void* );

    static void enable_interrupts( void (*isr)( void* ), void* that )
    {
        instance    = that;
        isr_handler = isr;

        NVIC_SetPriority( RADIO_IRQn, nrf_interrupt_prio_ble );
        NVIC_ClearPendingIRQ( RADIO_IRQn );
        NVIC_EnableIRQ( RADIO_IRQn );
    }

    static unsigned frequency_from_channel( unsigned channel )
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

    //////////////////////////////////////////
    // class counter
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

    //////////////////////////////////////////
    // class radio_hardware_without_crypto_support
    void radio_hardware_without_crypto_support::init( void (*isr)( void* ), void* that )
    {
        init_debug();

        init_radio( false );
        init_timer();
        init_ppi();

        receive_2mbit_  = false;
        transmit_2mbit_ = false;

        enable_interrupts( isr, that );
    }

    void radio_hardware_without_crypto_support::configure_radio_channel( unsigned channel )
    {
        // When the radio is ramping up for transmitting or reception and is then disabled,
        // the next state would be TXDISABLE or RXDISABLE. Wait till the radio safetely
        // entered DISABLED state
        while ((nrf_radio->STATE & RADIO_STATE_STATE_Msk) != RADIO_STATE_STATE_Disabled);

        nrf_radio->FREQUENCY   = frequency_from_channel( channel );
        nrf_radio->DATAWHITEIV = channel & 0x3F;

        nrf_radio->INTENCLR    = 0xffffffff;
    }

    void radio_hardware_without_crypto_support::configure_transmit_train(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_radio->MODE        =
            ( transmit_2mbit_ ? RADIO_MODE_MODE_Ble_2Mbit : RADIO_MODE_MODE_Ble_1Mbit ) << RADIO_MODE_MODE_Pos;

        nrf_radio->SHORTS      =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_RXEN_Msk
          | RADIO_SHORTS_ADDRESS_BCSTART_Msk;

        nrf_ppi->CHENCLR = all_preprogramed_ppi_channels_mask;
        nrf_ppi->CHENSET =
            ( 1 << radio_end_capture2_ppi_channel );

        nrf_radio->EVENTS_PAYLOAD = 0;
        nrf_radio->EVENTS_DISABLED = 0;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( transmit_data.size << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_without_crypto_support::configure_final_transmit(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        assert( transmit_data.buffer );

        nrf_radio->MODE        =
            ( transmit_2mbit_ ? RADIO_MODE_MODE_Ble_2Mbit : RADIO_MODE_MODE_Ble_1Mbit ) << RADIO_MODE_MODE_Pos;

        nrf_radio->SHORTS =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk;

        nrf_ppi->CHENCLR  = all_preprogramed_ppi_channels_mask;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( transmit_data.size << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_without_crypto_support::configure_receive_train(
        const bluetoe::link_layer::read_buffer& receive_buffer )
    {
        nrf_radio->MODE        =
            ( receive_2mbit_ ? RADIO_MODE_MODE_Ble_2Mbit : RADIO_MODE_MODE_Ble_1Mbit ) << RADIO_MODE_MODE_Pos;

        nrf_radio->SHORTS      =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_TXEN_Msk;

        nrf_ppi->CHENCLR = all_preprogramed_ppi_channels_mask;
        nrf_ppi->CHENSET       =
              ( 1 << compare0_rxen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 1 ] = 0;
        nrf_radio->EVENTS_PAYLOAD = 0;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer.size << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_without_crypto_support::stop_radio()
    {
        // disable interrupts
        nrf_radio->INTENCLR      = RADIO_INTENCLR_DISABLED_Msk;

        // Stop timer, which also could cause interrupts by disabling the radio
        nrf_timer->TASKS_STOP    = 1;
        nrf_timer->TASKS_CLEAR   = 1;

        // disconnect radio from all event sources
        nrf_ppi->CHENCLR         = all_radio_ppi_channels_mask;
        nrf_radio->SHORTS        = 0;

        // disable radio
        nrf_radio->TASKS_DISABLE = 1;
    }

    void radio_hardware_without_crypto_support::store_timer_anchor( int offset_us )
    {
        hf_connection_event_anchor_ = static_cast< int >( nrf_timer->CC[ tim_cc_capture_anchor ] ) + offset_us;
        lf_connection_event_anchor_ = nrf_rtc->CC[ rtc_cc_start_timer ];

        hf_user_timer_anchor_ = hf_connection_event_anchor_;
        lf_user_timer_anchor_ = lf_connection_event_anchor_;
        ++user_timer_anchor_version_;
    }

    std::pair< bool, bool > radio_hardware_without_crypto_support::received_pdu()
    {
        const bool result = ( nrf_radio->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) == RADIO_CRCSTATUS_CRCSTATUS_CRCOk && nrf_radio->EVENTS_PAYLOAD;
        nrf_radio->EVENTS_PAYLOAD = 0;

        return { result, result };
    }

    std::uint32_t radio_hardware_without_crypto_support::now()
    {
        std::uint32_t counter = nrf_rtc->COUNTER;

        if ( counter < lf_connection_event_anchor_ )
            counter += 0x1000000;

        const std::uint32_t anchor  = static_cast< std::int64_t >( lf_connection_event_anchor_ ) * 1000000 / nrf::lfxo_clk_freq + hf_connection_event_anchor_;
        const std::uint32_t current = static_cast< std::uint64_t >( counter ) * 1000000 / nrf::lfxo_clk_freq;

        // due to the lower resolution of current, current could possibly before anchor
        return anchor < current
            ? current - anchor
            : 0;
    }

    std::pair< bool, link_layer::delta_time > radio_hardware_without_crypto_support::can_stop_connection_event_timer( std::uint32_t safety_margin_us )
    {
        // the function must decide whether it is possible to stop the setup connection event before the
        // PPI machinery starts the high frequency oscilator. The decission has better to be on the
        // safe side, as otherwise, the radio might start transmitting unspecified content.
        const std::uint32_t counter = nrf_rtc->COUNTER;
        const std::uint32_t plan    = nrf_rtc->CC[ rtc_cc_start_hfxo ];

        // if counter > plan, then either the osc. was already started, or the counter wraped around
        const std::uint32_t distance = counter >= plan
            ? 0x1000000 + plan - counter
            : plan - counter;

        // if distance is larger that half the counter width, we probably have not a counter wrap
        if ( distance > 0x800000 )
            return { false, link_layer::delta_time() };

        const std::uint64_t time = static_cast< std::uint64_t >( distance ) * 1000000 / nrf::lfxo_clk_freq;

        // it is essentional to not underestimate the time from the anchor, otherwise, the setup of the
        // connection event will fail and result in a timeout, which would lead to an additional delay of
        // a interval.
        if ( time > safety_margin_us )
            return { true, link_layer::delta_time( now() + safety_margin_us ) };

        return { false, link_layer::delta_time() };
    }

    void radio_hardware_without_crypto_support::set_phy(
        bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t receiving_encoding,
        bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t transmiting_encoding )
    {
        if ( receiving_encoding != bluetoe::link_layer::details::phy_ll_encoding::le_unchanged_coding )
            receive_2mbit_ = receiving_encoding == bluetoe::link_layer::details::phy_ll_encoding::le_2m_phy;

        if ( transmiting_encoding != bluetoe::link_layer::details::phy_ll_encoding::le_unchanged_coding )
            transmit_2mbit_ = transmiting_encoding == bluetoe::link_layer::details::phy_ll_encoding::le_2m_phy;
    }

    static void setup_long_distance_timer(
        const int           hf_anchor,
        const std::uint32_t lf_anchor,
        const std::uint32_t us_radio_start_time,
        const std::uint32_t us_radio_startup_delay,
        const std::uint32_t us_radio_timeout,
        const bool          transmit,
        const std::uint32_t start_hfxo_offset )
    {
        // High frequency clock is running and is running from the crystal oscillator
        assert( ( nrf_clock->HFCLKSTAT & ( CLOCK_HFCLKSTAT_STATE_Msk | CLOCK_HFCLKSTAT_SRC_Msk ) )
          ==
          ( ( CLOCK_HFCLKSTAT_STATE_Running << CLOCK_HFCLKSTAT_STATE_Pos )
          | ( CLOCK_HFCLKSTAT_SRC_Xtal << CLOCK_HFCLKSTAT_SRC_Pos ) ) );

        // Stop timer, stop HFXO
        nrf_timer->TASKS_STOP = 1;
        nrf_timer->TASKS_CLEAR = 1;

        nrf_rtc->EVENTS_COMPARE[ rtc_cc_start_timer ] = 0;
        nrf_rtc->EVENTS_COMPARE[ rtc_cc_start_hfxo ] = 0;
        nrf_clock->EVENTS_HFCLKSTARTED = 0;
        nrf_timer->EVENTS_COMPARE[ tim_cc_start_radio ] = 0;
        nrf_timer->EVENTS_COMPARE[ tim_cc_timeout ] = 0;

        // This time in the LFCLK domain corresponds with 0 in the HFCLK domain
        const std::uint32_t rtc_tim_start_time  = lf_anchor;

        // -1 to prevent the calculation to endup with starting the Radio at TIMER0 beeing 0
        static constexpr auto us_offset   = 1;

        // This is the radio start time in the HFCLK domain, that is start_radio_time µs after rtc_tim_start_time
        // in the LFCLK domain.
        const std::uint32_t start_radio_time = hf_anchor + us_radio_start_time - us_radio_startup_delay - us_offset;

        // TODO: Optimize devision expressions for size
        const std::uint32_t lf_start_radio_time =
          static_cast< std::uint64_t >( start_radio_time )
        * static_cast< std::uint64_t >( nrf::lfxo_clk_freq ) / 1000000;

        const std::uint32_t hf_start_radio_time = start_radio_time -
          static_cast< std::uint64_t >( lf_start_radio_time )
        * static_cast< std::uint64_t >( 1000000 ) / nrf::lfxo_clk_freq;

        nrf_rtc->CC[ rtc_cc_start_timer ] = rtc_tim_start_time + lf_start_radio_time;
        nrf_rtc->CC[ rtc_cc_start_hfxo ]  = nrf_rtc->CC[ rtc_cc_start_timer ] - start_hfxo_offset;

        // +1 borrowed at the beginning of the caluculation to prevent this register from beeing 0
        nrf_timer->CC[ tim_cc_start_radio ] = hf_start_radio_time + us_offset;
        nrf_timer->CC[ tim_cc_timeout ]     = nrf_timer->CC[ tim_cc_start_radio ] + us_radio_startup_delay + us_radio_timeout;

        nrf_ppi->CHENSET =
            ( transmit ? transmit_ppi_channels : receive_ppi_channels )
          | ( 1 << rtc0_start_tim_ppi_channel )
          | ( 1 << rtc_start_hfxo_ppi_channel );

        record_long_distance_timer( hf_anchor, lf_anchor, us_radio_start_time, us_radio_startup_delay, us_radio_timeout );
    }

    static void enable_radio_disabled_interrupt()
    {
        nrf_radio->EVENTS_DISABLED = 0;
        nrf_radio->INTENSET = RADIO_INTENSET_DISABLED_Msk;
    }

    // TODO This will not work if Advertising is stopped for a larger amount of time. In this case
    //      the HFXO will run. Do we need to change the interface?
    bool radio_hardware_without_crypto_support::schedule_advertisment_event_timer(
        bluetoe::link_layer::delta_time when,
        std::uint32_t                   read_timeout_us,
        std::uint32_t                   start_hfxo_offset )
    {
        if ( when.zero() )
        {
            // immediately start the radio
            nrf_timer->TASKS_START = 1;
            nrf_timer->TASKS_CAPTURE[ tim_cc_timeout ]  = 1;
            nrf_timer->CC[ tim_cc_timeout ]            += us_radio_tx_startup_time + read_timeout_us;

            nrf_radio->TASKS_TXEN                       = 1;
        }
        else
        {
            // We need at least ~2ms to use this machinery
            assert( when.usec() > 2000 );

            // The Radio was planned to be started at this point, which is the anchor for the next
            // advertising
            const std::uint32_t hf_anchor = nrf_timer->CC[ tim_cc_start_radio ] + us_radio_tx_startup_time;
            const std::uint32_t lf_anchor = nrf_rtc->CC[ rtc_cc_start_timer ];

            setup_long_distance_timer(
                hf_anchor, lf_anchor, when.usec(), us_radio_tx_startup_time, read_timeout_us, true, start_hfxo_offset );
        }

        enable_radio_disabled_interrupt();

        return !when.zero();
    }

    void radio_hardware_without_crypto_support::schedule_connection_event_timer(
        std::uint32_t                   begin_us,
        std::uint32_t                   end_us,
        std::uint32_t                   start_hfxo_offset )
    {
        setup_long_distance_timer(
            hf_connection_event_anchor_,
            lf_connection_event_anchor_,
            begin_us, us_radio_rx_startup_time, end_us - begin_us, false, start_hfxo_offset );

        enable_radio_disabled_interrupt();
    }

    static void (*user_timer_isr_handler)( void* );

    bool radio_hardware_without_crypto_support::schedule_user_timer(
        void (*isr)( void* ),
        std::uint32_t                   time_us,
        std::uint32_t                   max_cb_runtimer_ms )
    {
        // when this assert hits, the last callback took longer than declared
        assert( !nrf_cb_timer->EVENTS_COMPARE[ cb_tim_cc_assert_end_cb ] );
        assert( max_cb_runtimer_ms > 0 );

        user_timer_isr_handler = isr;

        // configure timer and PPI. If the user timer is not used, the application is free to use
        // timer and PPI
        configure_timer_for_1us( *nrf_cb_timer );
        nrf_cb_timer->SHORTS   = TIMER_SHORTS_COMPARE1_STOP_Enabled << TIMER_SHORTS_COMPARE1_STOP_Pos;
        nrf_cb_timer->INTENSET = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
        nrf_rtc->EVTENSET = RTC_EVTEN_COMPARE2_Enabled << RTC_EVTEN_COMPARE2_Pos;

        assign_channel( rtc_start_cb_timer_ppi_channel, nrf_rtc->EVENTS_COMPARE[ rtc_cc_start_cb_timer ], nrf_cb_timer->TASKS_START );
        nrf_ppi->CHENSET = 1 << rtc_start_cb_timer_ppi_channel;

        NVIC_SetPriority( TIMER1_IRQn, nrf_interrupt_prio_user_cb );
        NVIC_ClearPendingIRQ( TIMER1_IRQn );
        NVIC_EnableIRQ( TIMER1_IRQn );

        int anchor_version;
        int hf_anchor;
        std::uint32_t lf_anchor;

        {
            lock_guard lock;
            anchor_version = user_timer_anchor_version_;
            hf_anchor = hf_user_timer_anchor_;
            lf_anchor = lf_user_timer_anchor_;
        }

        const auto counter = nrf_rtc->COUNTER;
        const auto anchor_distance = bluetoe::details::distance_n< 24u >( counter, lf_anchor );

        // The anchor was reset during the last connection event Simply wait for 2 * time_us
        // except for the very first call
        if ( static_cast< unsigned >( std::abs( anchor_distance ) ) > 5u && !user_timer_start_ )
        {
            time_us = 2 * time_us;
        }
        user_timer_start_ = false;

        // -1 to prevent the calculation to endup with starting the Radio at TIMER1 beeing 0
        static constexpr auto us_offset   = 1;

        const std::uint32_t trigger_isr_time = hf_anchor + time_us - us_offset;

        const std::uint32_t lf_call_cb_time =
          static_cast< std::uint64_t >( trigger_isr_time )
        * static_cast< std::uint64_t >( nrf::lfxo_clk_freq ) / 1000000;

        const std::uint32_t hf_call_cb_time = trigger_isr_time -
          static_cast< std::uint64_t >( lf_call_cb_time )
        * static_cast< std::uint64_t >( 1000000 ) / nrf::lfxo_clk_freq;

        nrf_rtc->CC[ rtc_cc_start_cb_timer ] = lf_anchor + lf_call_cb_time;

        assert( ( bluetoe::details::distance_n< 24u, std::uint32_t >( counter, nrf_rtc->CC[ rtc_cc_start_cb_timer ] ) > 0 ) );

        nrf_cb_timer->CC[ cb_tim_cc_start_exec_cb ] = hf_call_cb_time + us_offset;
        nrf_cb_timer->CC[ cb_tim_cc_assert_end_cb ] = nrf_cb_timer->CC[ cb_tim_cc_start_exec_cb ] + max_cb_runtimer_ms;

        assert( nrf_cb_timer->CC[ cb_tim_cc_assert_end_cb ] > nrf_cb_timer->CC[ cb_tim_cc_start_exec_cb ] );

        // set anchor to the destination time
        {
            lock_guard lock;

            if ( anchor_version == user_timer_anchor_version_ )
            {
                hf_user_timer_anchor_ = nrf_cb_timer->CC[ cb_tim_cc_start_exec_cb ];
                lf_user_timer_anchor_ = nrf_rtc->CC[ rtc_cc_start_cb_timer ];
            }
        }

        return true;
    }

    bool radio_hardware_without_crypto_support::stop_user_timer()
    {
        nrf_ppi->CHENCLR = 1 << rtc_start_cb_timer_ppi_channel;
        nrf_cb_timer->INTENCLR = TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos;
        nrf_cb_timer->TASKS_STOP = 1;
        nrf_cb_timer->INTENCLR = 0xFFFFFFFF;

        user_timer_start_ = true;

        const bool result = nrf_cb_timer->EVENTS_COMPARE[ cb_tim_cc_start_exec_cb ] == 0;
        nrf_cb_timer->EVENTS_COMPARE[ cb_tim_cc_start_exec_cb ] = 0;

        return result;
    }

    void radio_hardware_without_crypto_support::stop_timeout_timer()
    {
        nrf_ppi->CHENCLR = ( 1 << compare1_disable_ppi_channel );
    }

    std::uint32_t radio_hardware_without_crypto_support::static_random_address_seed()
    {
        return NRF_FICR->DEVICEID[ 0 ];
    }

    void radio_hardware_without_crypto_support::set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
    {
        nrf_radio->BASE0     = ( access_address << 8 ) & 0xFFFFFF00;
        nrf_radio->PREFIX0   = ( access_address >> 24 ) & RADIO_PREFIX0_AP0_Msk;
        nrf_radio->CRCINIT   = crc_init;
    }

    void radio_hardware_without_crypto_support::debug_toggle()
    {
        toggle_debug_pin();
    }


    bool                   radio_hardware_without_crypto_support::receive_2mbit_;
    bool                   radio_hardware_without_crypto_support::transmit_2mbit_;
    int                    radio_hardware_without_crypto_support::hf_connection_event_anchor_;
    std::uint32_t          radio_hardware_without_crypto_support::lf_connection_event_anchor_;
    volatile int           radio_hardware_without_crypto_support::hf_user_timer_anchor_;
    volatile std::uint32_t radio_hardware_without_crypto_support::lf_user_timer_anchor_;
    volatile int           radio_hardware_without_crypto_support::user_timer_anchor_version_;
    volatile bool          radio_hardware_without_crypto_support::user_timer_start_;

    //////////////////////////////////////////////
    // class radio_hardware_with_crypto_support
    static constexpr std::size_t ccm_key_offset = 0;
    static constexpr std::size_t ccm_packet_counter_offset = 16;
    static constexpr std::size_t ccm_packet_counter_size   = 5;
    static constexpr std::size_t ccm_direction_offset = 24;
    static constexpr std::size_t ccm_iv_offset  = 25;

    static constexpr std::uint8_t central_to_peripheral_ccm_direction = 0x01;
    static constexpr std::uint8_t peripheral_to_central_ccm_direction = 0x00;

    // the value MAXPACKETSIZE from the documentation seems to be the maximum value, the size field can store,
    // and is independent from the MTU size (https://devzone.nordicsemi.com/f/nordic-q-a/13123/what-is-actual-size-required-for-scratch-area-for-ccm-on-nrf52/50031#50031)
    static constexpr std::size_t scratch_size   = 267;

    static struct alignas( 4 ) ccm_data_struct_t {
        std::uint8_t data[ 33 ];
    } ccm_data_struct;

    static struct alignas( 4 ) scratch_area_t {
        std::uint8_t data[ scratch_size ];
    } scratch_area;

    static void setup_ccm_data_structure( const bluetoe::details::uint128_t& key, std::uint64_t IV )
    {
        std::copy( key.rbegin(), key.rend(), &ccm_data_struct.data[ ccm_key_offset ] );
        std::fill( &ccm_data_struct.data[ ccm_packet_counter_offset ],
            &ccm_data_struct.data[ ccm_packet_counter_offset + ccm_packet_counter_size ], 0 );

        details::write_64bit( &ccm_data_struct.data[ ccm_iv_offset ], IV );
    }

    void radio_hardware_with_crypto_support::init( std::uint8_t* encrypted_area, void (*isr)( void* ), void* that )
    {
        init_debug();

        encrypted_area_ = encrypted_area;

        init_radio( true );
        init_timer();
        init_ppi();

        enable_interrupts( isr, that );

        nrf_random->CONFIG  = RNG_CONFIG_DERCEN_Msk;
        nrf_random->SHORTS  = RNG_SHORTS_VALRDY_STOP_Msk;

        nrf_ccm->SCRATCHPTR = reinterpret_cast< std::uintptr_t >( &scratch_area );
        nrf_ccm->CNFPTR     = reinterpret_cast< std::uintptr_t >( &ccm_data_struct );

        configure_encryption( false, false );
    }

    void radio_hardware_with_crypto_support::configure_transmit_train(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_radio->MODE        =
            ( transmit_2mbit_ ? RADIO_MODE_MODE_Ble_2Mbit : RADIO_MODE_MODE_Ble_1Mbit ) << RADIO_MODE_MODE_Pos;

        nrf_radio->SHORTS      =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_RXEN_Msk
          | RADIO_SHORTS_ADDRESS_BCSTART_Msk;

        nrf_ppi->CHENCLR = all_preprogramed_ppi_channels_mask;
        nrf_ppi->CHENSET =
            ( 1 << radio_end_capture2_ppi_channel );

        nrf_radio->EVENTS_PAYLOAD = 0;
        nrf_radio->EVENTS_DISABLED = 0;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
        nrf_radio->PCNF1       =
            ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk )
          | ( ( transmit_data.size - 1 ) << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_with_crypto_support::configure_final_transmit(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_radio->MODE        =
            ( transmit_2mbit_ ? RADIO_MODE_MODE_Ble_2Mbit : RADIO_MODE_MODE_Ble_1Mbit ) << RADIO_MODE_MODE_Pos;

        nrf_ppi->CHENCLR        = all_preprogramed_ppi_channels_mask;

        nrf_radio->SHORTS =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk;

        // only encrypt none empty PDUs
        if ( transmit_encrypted_ && transmit_data.buffer[ 1 ] != 0 )
        {
            transmit_counter_.copy_to( &ccm_data_struct.data[ ccm_packet_counter_offset ] );
            ccm_data_struct.data[ ccm_direction_offset ] = peripheral_to_central_ccm_direction;

            nrf_ccm->SHORTS  = CCM_SHORTS_ENDKSGEN_CRYPT_Msk;
            nrf_ccm->MODE    =
                  ( CCM_MODE_MODE_Encryption << CCM_MODE_MODE_Pos )
                | ( CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos )
                | ( ( transmit_2mbit_ ? CCM_MODE_DATARATE_2Mbit : CCM_MODE_DATARATE_1Mbit ) << CCM_MODE_DATARATE_Pos );
            nrf_ccm->OUTPTR     = reinterpret_cast< std::uint32_t >( encrypted_area_ );
            nrf_ccm->INPTR      = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
            nrf_ccm->SCRATCHPTR = reinterpret_cast< std::uintptr_t >( &scratch_area );

            nrf_ccm->EVENTS_ENDKSGEN    = 0;
            nrf_ccm->EVENTS_ENDCRYPT    = 0;

            nrf_radio->PACKETPTR = reinterpret_cast< std::uint32_t >( encrypted_area_ );
            nrf_radio->PCNF1 =
                ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk )
              | ( ( transmit_data.size + encryption_mic_size - 1 ) << RADIO_PCNF1_MAXLEN_Pos );

            nrf_ccm->TASKS_KSGEN        = 1;
        }
        else
        {
            nrf_radio->PACKETPTR    = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
            nrf_radio->PCNF1        =
                ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk )
              | ( ( transmit_data.size - 1 ) << RADIO_PCNF1_MAXLEN_Pos );
        }
    }

    void radio_hardware_with_crypto_support::configure_receive_train(
        const bluetoe::link_layer::read_buffer&     receive_buffer )
    {
        nrf_radio->MODE        =
            ( receive_2mbit_ ? RADIO_MODE_MODE_Ble_2Mbit : RADIO_MODE_MODE_Ble_1Mbit ) << RADIO_MODE_MODE_Pos;

        if ( receive_encrypted_ )
        {
            receive_counter_.copy_to( &ccm_data_struct.data[ ccm_packet_counter_offset ] );
            ccm_data_struct.data[ ccm_direction_offset ] = central_to_peripheral_ccm_direction;

            // Reseting the CCM before every connection event seems to workaround a bug that
            // Causes the CCM to start decrypting an incomming PDU, before it was actually received
            // which causes overwriting the receive buffer, if the length field in the encrypted_area_
            // was long enough.
            // (https://devzone.nordicsemi.com/f/nordic-q-a/43656/what-causes-decryption-before-receiving)
            nrf_ccm->ENABLE = CCM_ENABLE_ENABLE_Disabled;
            nrf_ccm->ENABLE = CCM_ENABLE_ENABLE_Enabled;
            nrf_ccm->MODE   =
                  ( CCM_MODE_MODE_Decryption << CCM_MODE_MODE_Pos )
                | ( CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos )
                | ( ( receive_2mbit_ ? CCM_MODE_DATARATE_2Mbit : CCM_MODE_DATARATE_1Mbit ) << CCM_MODE_DATARATE_Pos );
            nrf_ccm->CNFPTR     = reinterpret_cast< std::uintptr_t >( &ccm_data_struct );
            nrf_ccm->INPTR      = reinterpret_cast< std::uint32_t >( encrypted_area_ );
            nrf_ccm->OUTPTR     = reinterpret_cast< std::uint32_t >( receive_buffer.buffer );
            nrf_ccm->SCRATCHPTR = reinterpret_cast< std::uintptr_t >( &scratch_area );
            nrf_ccm->SHORTS     = 0;
            nrf_ccm->EVENTS_ENDKSGEN = 0;
            nrf_ccm->EVENTS_ENDCRYPT = 0;
            nrf_ccm->EVENTS_ERROR = 0;

            nrf_ccm->TASKS_KSGEN = 1;
        }

        static constexpr std::uint32_t radio_shorts =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_TXEN_Msk;

        static constexpr std::uint32_t encrypted_ppi_channels =
            ( 1 << radio_address_ccm_crypt )
          | ( 1 << compare0_rxen_ppi_channel )
          | ( 1 << compare1_disable_ppi_channel )
          | ( 1 << radio_end_capture2_ppi_channel );

        static constexpr std::uint32_t unencrypted_ppi_channels =
            ( 1 << compare0_rxen_ppi_channel )
          | ( 1 << compare1_disable_ppi_channel )
          | ( 1 << radio_end_capture2_ppi_channel );

        nrf_radio->SHORTS   = radio_shorts;
        nrf_ppi->CHENCLR    = all_preprogramed_ppi_channels_mask;
        nrf_ppi->CHENSET    = receive_encrypted_
            ? encrypted_ppi_channels
            : unencrypted_ppi_channels;

        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 1 ] = 0;
        nrf_radio->EVENTS_PAYLOAD = 0;

        const auto mic_size = []( bool receive_encrypted ){
            return receive_encrypted
                ? encryption_mic_size
                : 0;
        };

        nrf_radio->PACKETPTR   = receive_encrypted_
            ? reinterpret_cast< std::uint32_t >( encrypted_area_ )
            : reinterpret_cast< std::uint32_t >( receive_buffer.buffer );

        nrf_radio->PCNF1       =
            ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk )
          | ( ( receive_buffer.size + mic_size( receive_encrypted_ ) ) << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_with_crypto_support::store_timer_anchor( int offset_us )
    {
        // TODO: Hack is required, as we capture the Anchor at the END of the PDU
        // Issue: #76 Taking Anchor from End of PDU
        if ( receive_encrypted_ && offset_us < -80 )
            offset_us -= encryption_mic_size * 8;

        radio_hardware_without_crypto_support::store_timer_anchor( offset_us );
    }

    std::pair< bool, bool > radio_hardware_with_crypto_support::received_pdu()
    {
        const auto receive_size = [](){
            return reinterpret_cast< const std::uint8_t* >( nrf_radio->PACKETPTR )[ 1 ];
        };

        const bool timeout      = ( nrf_radio->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) != RADIO_CRCSTATUS_CRCSTATUS_CRCOk || !nrf_radio->EVENTS_PAYLOAD;
        const bool crc_error    = !timeout && ( nrf_radio->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) != RADIO_CRCSTATUS_CRCSTATUS_CRCOk;
        const bool mic_error    = receive_encrypted_ && receive_size() != 0 && ( nrf_ccm->MICSTATUS & CCM_MICSTATUS_MICSTATUS_Msk ) == CCM_MICSTATUS_MICSTATUS_CheckFailed;
        const bool bus_error    = receive_encrypted_ && nrf_ccm->EVENTS_ERROR;
        const bool not_decrypt  = receive_encrypted_ && receive_size() != 0 && nrf_ccm->EVENTS_ENDCRYPT == 0;

        const bool valid_anchor = !timeout && !bus_error;
        const bool valid_pdu    = valid_anchor && !crc_error && !not_decrypt && !mic_error;

        nrf_radio->EVENTS_PAYLOAD = 0;

        return { valid_anchor, valid_pdu };
    }

    void radio_hardware_with_crypto_support::configure_encryption( bool receive, bool transmit )
    {
        if ( receive && transmit )
            transmit_counter_ = counter();

        if ( receive && !transmit )
            receive_counter_ = counter();

        if ( !receive && !transmit )
        {
            // delete key out of memory
            std::memset( &ccm_data_struct, 0, sizeof( ccm_data_struct ) );
            nrf_ccm->ENABLE = nrf_ccm->ENABLE & ~CCM_ENABLE_ENABLE_Msk;
        }

        receive_encrypted_ = receive;
        transmit_encrypted_ = transmit;
    }

    std::pair< std::uint64_t, std::uint32_t > radio_hardware_with_crypto_support::setup_encryption( bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm )
    {
        const std::uint64_t skds = random_number64();
        const std::uint32_t ivs  = random_number32();

        bluetoe::details::uint128_t session_descriminator;
        details::write_64bit( &session_descriminator[ 0 ], skdm );
        details::write_64bit( &session_descriminator[ 8 ], skds );

        setup_ccm_data_structure(
            aes_le( key, session_descriminator ),
            static_cast< std::uint64_t >( ivm ) | ( static_cast< std::uint64_t >( ivs ) << 32 ) );

        return { skds, ivs };
    }

    void radio_hardware_with_crypto_support::setup_identity_resolving_address(
        const std::uint8_t* address )
    {
        if ( identity_resolving_enabled_ )
        {
            nrf_aar->EVENTS_END         = 0;
            nrf_aar->EVENTS_RESOLVED    = 0;
            nrf_aar->EVENTS_NOTRESOLVED = 0;

            nrf_aar->ADDRPTR = reinterpret_cast< std::uint32_t >( address ) ;
        }
    }

    void radio_hardware_with_crypto_support::set_identity_resolving_key(
        const details::identity_resolving_key_t& irk )
    {
        static details::identity_resolving_key_t irk_storage;
        irk_storage = irk;

        identity_resolving_enabled_ = true;

        // disable CCM
        nrf_ccm->ENABLE   = CCM_ENABLE_ENABLE_Disabled;
        nrf_ccm->INTENCLR = 0xFFFFFFFF;
        nrf_ppi->CHENCLR  = ( 1 << radio_address_ccm_crypt );

        // setup AAR
        nrf_ppi->CHENSET    = ( 1 << radio_bcmatch_aar_start_channel );
        nrf_radio->BCC      = 16 + 2 * ( 6 * 8 );
        nrf_aar->SCRATCHPTR = reinterpret_cast< std::uintptr_t >( &scratch_area );
        nrf_aar->IRKPTR     = reinterpret_cast< std::uint32_t >( &irk_storage );
        nrf_aar->NIRK       = 1;

        nrf_aar->ENABLE     = AAR_ENABLE_ENABLE_Msk;
    }

    bool radio_hardware_with_crypto_support::resolving_address_invalid()
    {
        if ( identity_resolving_enabled_ )
        {
            while ( !nrf_aar->EVENTS_END )
                ;

            if ( nrf_aar->EVENTS_NOTRESOLVED )
                return true;
        }

        return false;
    }

    volatile bool radio_hardware_with_crypto_support::receive_encrypted_  = false;
    volatile bool radio_hardware_with_crypto_support::transmit_encrypted_ = false;
    std::uint8_t* radio_hardware_with_crypto_support::encrypted_area_;
    counter radio_hardware_with_crypto_support::transmit_counter_;
    counter radio_hardware_with_crypto_support::receive_counter_;
    bool    radio_hardware_with_crypto_support::identity_resolving_enabled_ = false;

    // Problem: The HFXO is required by the radio and by the calibration.
    // For the Radio, the HFXO is requested by an RTC event. So there is no mean, to
    // increment some kind of usage counter that could be used to defere the disabling
    // of the HFXO if one of the two usages are still using the HFXO.

    // Solution: Let the radio still start / stop the HFXO, but defere the stop to the
    // next connection event, if the HFXO is still in use by the calibration.

    // Design: nrf_clock->EVENTS_CTTO is used to indicate, that a calibration of the
    // RC is requested. The calibration timeout does not start the HFXO, it's waited
    // until the next connection events starts the HFXO. The HFXO is then stopped,
    // by the end of a connection event, after the calibration was done.

    // As there is a requirement, to have the calibration run at least every 8 seconds,
    // and as the maximum advertising event is 4s, it is required to have the calibration
    // running every 4s.
    static constexpr std::uint32_t calibration_timer_counter = 4 * 4;
    static bool calibration_running = false;

    static std::uint32_t last_calibration_temp = 0;
    static constexpr int calibration_temp_threshold = 2; // 0.5°

    // this indirection leads to clock_calibrate_isr() not beeing linked in, if not used
    static void (*clock_isr_handler)() = nullptr;
    static void clock_calibrate_isr()
    {
        set_isr_pin();

        // Calibration Timer Time Out
        if ( nrf_clock->EVENTS_CTTO )
        {
            nrf_clock->INTENCLR = CLOCK_INTENSET_CTTO_Msk;
        }

        // Calibration timer timed out and HFXO is running
        if ( nrf_clock->EVENTS_HFCLKSTARTED )
        {
            nrf_clock->EVENTS_HFCLKSTARTED = 0;

            nrf_temp->EVENTS_DATARDY = 0;
            nrf_temp->TASKS_START = 1;

            if ( nrf_clock->EVENTS_CTTO && !calibration_running )
            {
                calibration_running = true;
                nrf_clock->TASKS_CAL = 1;
            }
        }

        // Calibration Done
        if ( nrf_clock->EVENTS_DONE )
        {
            calibration_running = false;
            nrf_clock->EVENTS_DONE = 0;
            nrf_clock->EVENTS_CTTO = 0;
            nrf_clock->INTENSET  = CLOCK_INTENSET_CTTO_Msk;

            // Restart calibration timer
            nrf_clock->CTIV = calibration_timer_counter;
            nrf_clock->TASKS_CTSTART = 1;
        }

        reset_isr_pin();
    }

    void init_calibration_timer()
    {
        clock_isr_handler = clock_calibrate_isr;

        nrf_clock->EVENTS_DONE = 0;
        nrf_clock->EVENTS_CTTO = 0;
        nrf_clock->EVENTS_HFCLKSTARTED = 0;

        nrf_clock->INTENSET =
            CLOCK_INTENSET_DONE_Msk
          | CLOCK_INTENSET_CTTO_Msk
          | CLOCK_INTENSET_HFCLKSTARTED_Msk;

        NVIC_SetPriority( POWER_CLOCK_IRQn, nrf_interrupt_prio_calibrate_rtc );
        NVIC_ClearPendingIRQ( POWER_CLOCK_IRQn );
        NVIC_EnableIRQ( POWER_CLOCK_IRQn );

        calibration_running = false;

        nrf_clock->CTIV = 0;
        nrf_clock->TASKS_CTSTART = 1;
    }

    void deassign_hfxo()
    {
        if ( nrf_clock->EVENTS_CTTO == 0 )
        {
            nrf_clock->TASKS_HFCLKSTOP = 1;
            gpio_debug_hfxo_stopped();
        }

        if ( nrf_temp->EVENTS_DATARDY )
        {
            nrf_temp->EVENTS_DATARDY = 0;

            const int current_temp = static_cast< int >( nrf_temp->TEMP );
            const int diff = last_calibration_temp - current_temp;

            if ( diff >= calibration_temp_threshold || diff <= -calibration_temp_threshold )
            {
                last_calibration_temp = current_temp;

                // force a recalibration at the next connection event
                nrf_clock->TASKS_CTSTOP = 1;
                nrf_clock->CTIV = 0;
                nrf_clock->TASKS_CTSTART = 1;
            }
        }
    }
} // namespace nrf52_details
} // namespace bluetoe

extern "C" void RADIO_IRQHandler(void)
{
    using namespace bluetoe::nrf52_details;
    set_isr_pin();

    assert( bluetoe::nrf::nrf_radio->EVENTS_DISABLED );
    // High frequency clock is running and is running from the crystal oscilator
    assert( ( bluetoe::nrf::nrf_clock->HFCLKSTAT & ( CLOCK_HFCLKSTAT_SRC_Msk | CLOCK_HFCLKSTAT_STATE_Msk ) )
        == ( ( CLOCK_LFCLKSTAT_STATE_Running << CLOCK_LFCLKSTAT_STATE_Pos ) | ( CLOCK_LFCLKSTAT_SRC_Xtal << CLOCK_LFCLKSTAT_SRC_Pos ) ) );

    assert( bluetoe::nrf52_details::instance );
    assert( bluetoe::nrf52_details::isr_handler );
    isr_handler( bluetoe::nrf52_details::instance );

    bluetoe::nrf::nrf_radio->EVENTS_END       = 0;
    bluetoe::nrf::nrf_radio->EVENTS_DISABLED  = 0;
    bluetoe::nrf::nrf_radio->EVENTS_READY     = 0;
    bluetoe::nrf::nrf_radio->EVENTS_ADDRESS   = 0;
    bluetoe::nrf::nrf_radio->EVENTS_PAYLOAD   = 0;

    reset_isr_pin();
}

extern "C" void POWER_CLOCK_IRQHandler()
{
    bluetoe::nrf52_details::clock_isr_handler();
}

extern "C" void TIMER1_IRQHandler()
{
    assert( bluetoe::nrf52_details::instance );
    assert( bluetoe::nrf52_details::user_timer_isr_handler );

    bluetoe::nrf::nrf_cb_timer->TASKS_STOP = 1;
    bluetoe::nrf::nrf_cb_timer->INTENCLR = 0xFFFFFFFF;
    bluetoe::nrf::nrf_cb_timer->EVENTS_COMPARE[ bluetoe::nrf52_details::cb_tim_cc_start_exec_cb ] = 0;

    // the next timer is setup from within the callback,
    // so better do not alter anything after this call
    bluetoe::nrf52_details::user_timer_isr_handler( bluetoe::nrf52_details::instance );
}
