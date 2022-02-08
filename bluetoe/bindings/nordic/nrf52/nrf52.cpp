#include <bluetoe/nrf52.hpp>

namespace bluetoe
{
namespace nrf52_details
{
    static constexpr std::size_t        radio_address_ccm_crypt         = 25;
    static constexpr std::size_t        radio_end_capture2_ppi_channel  = 27;
    static constexpr std::size_t        compare0_txen_ppi_channel       = 20;
    static constexpr std::size_t        compare0_rxen_ppi_channel       = 21;
    static constexpr std::size_t        compare1_disable_ppi_channel    = 22;
    static constexpr std::size_t        radio_bcmatch_aar_start_channel = 23;

#   if defined BLUETOE_NRF52_RADIO_DEBUG
        static constexpr int debug_pin_end_crypt     = 11;
        static constexpr int debug_pin_ready_disable = 12;
        static constexpr int debug_pin_address_end   = 13;
        static constexpr int debug_pin_keystream     = 14;
        static constexpr int debug_pin_debug         = 15;

        static void init_debug()
        {
            for ( auto pin : { debug_pin_end_crypt, debug_pin_ready_disable,
                                debug_pin_address_end, debug_pin_keystream, debug_pin_debug } )
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


            NRF_PPI->CH[ 0 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_ADDRESS );
            NRF_PPI->CH[ 0 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 0 ] );

            NRF_PPI->CH[ 1 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_END );
            NRF_PPI->CH[ 1 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 0 ] );

            NRF_PPI->CH[ 2 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_CCM->EVENTS_ENDCRYPT );
            NRF_PPI->CH[ 2 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 3 ] );

            NRF_PPI->CH[ 3 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_CCM->EVENTS_ENDKSGEN );
            NRF_PPI->CH[ 3 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_CLR[ 1 ] );

            NRF_PPI->CH[ 4 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_READY );
            NRF_PPI->CH[ 4 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 2 ] );

            NRF_PPI->CH[ 5 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_DISABLED );
            NRF_PPI->CH[ 5 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 2 ] );

            NRF_PPI->CHENSET = 0x3f;
        }

#   else
        static void init_debug() {}
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
        nrf_ppi->CHENCLR =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare0_rxen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel )
            | ( 1 << radio_bcmatch_aar_start_channel );

        // The polynomial has the form of x^24 +x^10 +x^9 +x^6 +x^4 +x^3 +x+1
        nrf_radio->CRCPOLY   = 0x100065B;

        // TIFS is only enforced if END_DISABLE and DISABLED_TXEN shortcuts are enabled.
        nrf_radio->TIFS      = 150;
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
    // radio_hardware_without_crypto_support

    static void* instance = nullptr;
    static void (*isr_handler)( void* );

    void radio_hardware_without_crypto_support::init( void (*isr)( void* ), void* that )
    {
        instance    = that;
        isr_handler = isr;

        // start high freuquence clock source if not done yet
        if ( !NRF_CLOCK->EVENTS_HFCLKSTARTED )
        {
            NRF_CLOCK->TASKS_HFCLKSTART = 1;

            // TODO: do not wait busy
            while ( !NRF_CLOCK->EVENTS_HFCLKSTARTED )
                ;
        }

        init_debug();
        init_radio( false );
        init_timer();

        NVIC_SetPriority( RADIO_IRQn, 0 );
        NVIC_ClearPendingIRQ( RADIO_IRQn );
        NVIC_EnableIRQ( RADIO_IRQn );
    }

    void radio_hardware_without_crypto_support::configure_radio_channel(
        unsigned                                    channel )
    {
        // TODO Why do we need that?
        while ((nrf_radio->STATE & RADIO_STATE_STATE_Msk) != RADIO_STATE_STATE_Disabled);
        assert( ( nrf_radio->STATE & RADIO_STATE_STATE_Msk ) == RADIO_STATE_STATE_Disabled );

        nrf_radio->FREQUENCY   = frequency_from_channel( channel );
        nrf_radio->DATAWHITEIV = channel & 0x3F;

        nrf_radio->INTENCLR    = 0xffffffff;
        nrf_timer->INTENCLR    = 0xffffffff;

        nrf_radio->EVENTS_END       = 0;
        nrf_radio->EVENTS_DISABLED  = 0;
        nrf_radio->EVENTS_READY     = 0;
        nrf_radio->EVENTS_ADDRESS   = 0;
        nrf_radio->EVENTS_PAYLOAD   = 0;
    }

    void radio_hardware_without_crypto_support::configure_transmit_train(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_radio->SHORTS      =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_RXEN_Msk
          | RADIO_SHORTS_ADDRESS_BCSTART_Msk;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( transmit_data.size << RADIO_PCNF1_MAXLEN_Pos );

        nrf_ppi->CHENCLR = ( 1 << compare0_rxen_ppi_channel );
        nrf_ppi->CHENSET =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        nrf_radio->INTENSET    = RADIO_INTENSET_DISABLED_Msk;
    }

    void radio_hardware_without_crypto_support::configure_final_transmit(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_radio->SHORTS =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk;

        nrf_ppi->CHENCLR  =
            ( 1 << compare0_txen_ppi_channel )
          | ( 1 << compare1_disable_ppi_channel )
          | ( 1 << radio_end_capture2_ppi_channel );

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( transmit_data.size << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_without_crypto_support::configure_receive_train(
        const bluetoe::link_layer::read_buffer& receive_buffer )
    {
        nrf_radio->SHORTS      =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_TXEN_Msk;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer.size << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_without_crypto_support::stop_radio()
    {
        nrf_radio->SHORTS        = 0;
        nrf_radio->TASKS_DISABLE = 1;
    }

    void radio_hardware_without_crypto_support::store_timer_anchor()
    {
        anchor_offset_ = link_layer::delta_time( nrf_timer->CC[ 2 ] );
    }

    bool radio_hardware_without_crypto_support::received_pdu()
    {
        const bool result = ( nrf_radio->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) == RADIO_CRCSTATUS_CRCSTATUS_CRCOk && nrf_radio->EVENTS_PAYLOAD;
        nrf_radio->EVENTS_PAYLOAD = 0;

        return result;
    }

    void radio_hardware_without_crypto_support::schedule_radio_start(
        bluetoe::link_layer::delta_time when,
        std::uint32_t                   read_timeout_us )
    {
        if ( when.zero() )
        {
            nrf_radio->TASKS_TXEN          = 1;
            nrf_timer->TASKS_CAPTURE[ 1 ]  = 1;
            nrf_timer->CC[ 1 ]            += read_timeout_us + us_radio_tx_startup_time;
        }
        else
        {
            nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
            nrf_timer->CC[ 0 ]             = when.usec() - us_radio_tx_startup_time + anchor_offset_.usec();
            nrf_timer->CC[ 1 ]             = nrf_timer->CC[ 0 ] + us_radio_tx_startup_time + read_timeout_us;
            nrf_timer->CC[ 2 ]             = 0;

            // manually triggering event for timer beeing already behind target time
            nrf_timer->TASKS_CAPTURE[ 3 ]  = 1;

            if ( nrf_timer->EVENTS_COMPARE[ 0 ] || nrf_timer->CC[ 3 ] >= nrf_timer->CC[ 0 ] )
            {
                nrf_timer->TASKS_CLEAR = 1;
                nrf_radio->TASKS_TXEN = 1;
            }
        }
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

    // see https://devzone.nordicsemi.com/question/47493/disable-interrupts-and-enable-interrupts-if-they-where-enabled/
    radio_hardware_without_crypto_support::lock_guard::lock_guard()
        : context_( __get_PRIMASK() )
    {
        __disable_irq();
    }

    radio_hardware_without_crypto_support::lock_guard::~lock_guard()
    {
        __set_PRIMASK( context_ );
    }

    bluetoe::link_layer::delta_time radio_hardware_without_crypto_support::anchor_offset_;


} // namespace nrf52_details
} // namespace bluetoe

extern "C" void RADIO_IRQHandler(void)
{
    assert( bluetoe::nrf52_details::nrf_radio->EVENTS_DISABLED );

    bluetoe::nrf52_details::nrf_radio->EVENTS_DISABLED = 0;
    bluetoe::nrf52_details::nrf_radio->EVENTS_READY    = 0;

    assert( bluetoe::nrf52_details::instance );
    assert( bluetoe::nrf52_details::isr_handler );

    bluetoe::nrf52_details::isr_handler( bluetoe::nrf52_details::instance );
}
