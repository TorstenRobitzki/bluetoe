#include "tester/platform.hpp"

#include <nrf.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace bluetoe {
namespace test_rig {

    namespace {

        /*
         * P0.03: free on the nRF52840-DK and the nRF52-DK alike, on the analog header of
         * both.
         */
        constexpr std::uint32_t pin_reset = 3;

        /*
         * The CPU runs at 64 MHz whatever clock source drives it, so SysTick counts
         * milliseconds without any setup of the clocks.
         */
        constexpr std::uint32_t cpu_ticks_per_ms = 64000;
        constexpr std::uint32_t reset_hold_ms    = 5;

        void wait_ms( std::uint32_t ms )
        {
            SysTick->LOAD = cpu_ticks_per_ms - 1;
            SysTick->VAL  = 0;
            SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

            for ( ; ms; --ms )
                while ( !( SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk ) )
                    ;

            SysTick->CTRL = 0;
        }

        /*
         * TIMER0 counts the tester's ticks: prescaler 0 gives the full 16 MHz of the
         * peripheral clock, 62.5 ns a tick (decision 24). It free runs from boot, so a
         * capture is a time since boot; 32 bit wraps every 268 s, far beyond any run.
         */
        constexpr std::uint32_t prescaler_for_16mhz = 0;
        constexpr std::size_t   cc_address          = 0;    // the access address was received
        constexpr std::size_t   cc_window           = 1;    // the listen window is over
        constexpr std::size_t   cc_now              = 2;    // reading the clock

        /*
         * One free PPI channel captures the timer at the RADIO ADDRESS event.
         */
        constexpr std::size_t   ppi_address_capture = 0;

        /*
         * The RADIO ADDRESS event fires once the preamble and the access address are on
         * air, so the first bit of the packet was one preamble and one access address
         * earlier: 8 + 32 bits at 1 Mbit, 40 microseconds, 640 ticks. The value only
         * shifts the origin, which cancels in the interval a test compares, but it is the
         * true offset for an absolute measurement such as T_IFS.
         */
        constexpr std::uint32_t preamble_and_access_address_ticks = 40 * 16;

        std::uint32_t frequency_from_channel( std::uint32_t channel )
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

        void configure_radio()
        {
            NRF_RADIO->MODE     = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;
            NRF_RADIO->MODECNF0 =
                  ( RADIO_MODECNF0_RU_Fast << RADIO_MODECNF0_RU_Pos )
                | ( RADIO_MODECNF0_DTX_Center << RADIO_MODECNF0_DTX_Pos );

            // a legacy PDU: one byte of flags, one byte of length, eight bit preamble
            NRF_RADIO->PCNF0 =
                  ( 1 << RADIO_PCNF0_S0LEN_Pos )
                | ( 8 << RADIO_PCNF0_LFLEN_Pos )
                | ( 0 << RADIO_PCNF0_S1LEN_Pos )
                | ( RADIO_PCNF0_PLEN_8bit << RADIO_PCNF0_PLEN_Pos );

            NRF_RADIO->PCNF1 =
                  ( RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos )
                | ( RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos )
                | ( 3 << RADIO_PCNF1_BALEN_Pos )
                | ( 0 << RADIO_PCNF1_STATLEN_Pos )
                | ( ( max_advertising_pdu_size - 2 ) << RADIO_PCNF1_MAXLEN_Pos );

            NRF_RADIO->RXADDRESSES = 1 << 0;

            NRF_RADIO->CRCCNF =
                  ( RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos )
                | ( RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos );
            // x^24 + x^10 + x^9 + x^6 + x^4 + x^3 + x + 1
            NRF_RADIO->CRCPOLY  = 0x100065B;
        }
    }

    platform* platform::instance_ = nullptr;

    platform::platform()
        : event_head_( 0 )
        , event_tail_( 0 )
    {
        assert( instance_ == nullptr );
        instance_ = this;

        // released before the pin becomes an output, so that it never drives low by accident
        NRF_P0->OUTSET = 1u << pin_reset;
        NRF_P0->PIN_CNF[ pin_reset ] =
              ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos )
            | ( GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos )
            | ( GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos );

        NRF_CLOCK->EVENTS_HFCLKSTARTED  = 0;
        NRF_CLOCK->TASKS_HFCLKSTART     = 1;
        while ( !NRF_CLOCK->EVENTS_HFCLKSTARTED )
            ;

        NRF_TIMER0->TASKS_STOP  = 1;
        NRF_TIMER0->TASKS_CLEAR = 1;
        NRF_TIMER0->MODE        = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
        NRF_TIMER0->BITMODE     = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
        NRF_TIMER0->PRESCALER   = prescaler_for_16mhz;
        NRF_TIMER0->INTENCLR    = 0xffffffff;
        NRF_TIMER0->TASKS_START = 1;

        configure_radio();
        set_access_address_and_crc_init( 0x8E89BED6, 0x555555 );

        NRF_PPI->CH[ ppi_address_capture ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_ADDRESS );
        NRF_PPI->CH[ ppi_address_capture ].TEP = reinterpret_cast< std::uint32_t >( &NRF_TIMER0->TASKS_CAPTURE[ cc_address ] );
        NRF_PPI->CHENSET = 1u << ppi_address_capture;

        NVIC_SetPriority( RADIO_IRQn, 0 );
        NVIC_ClearPendingIRQ( RADIO_IRQn );
        NVIC_EnableIRQ( RADIO_IRQn );

        NVIC_SetPriority( TIMER0_IRQn, 0 );
        NVIC_ClearPendingIRQ( TIMER0_IRQn );
        NVIC_EnableIRQ( TIMER0_IRQn );
    }

    void platform::reset_device_under_test()
    {
        NRF_P0->OUTCLR = 1u << pin_reset;
        wait_ms( reset_hold_ms );
        NRF_P0->OUTSET = 1u << pin_reset;
    }

    void platform::run()
    {
        __WFE();
    }

    void platform::wake_up()
    {
        __SEV();
    }

    void platform::set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
    {
        NRF_RADIO->BASE0    = access_address << 8;
        NRF_RADIO->PREFIX0  = access_address >> 24;
        NRF_RADIO->CRCINIT  = crc_init;
    }

    /*
     * Listen on the channel until the window is over, restarting the receiver after every
     * packet so that a whole window of PDUs is caught. Only 1 Mbit is implemented; the phy
     * is ignored until a test needs another.
     */
    void platform::receive( std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t, std::uint64_t ticks )
    {
        // bring the radio to DISABLED, whatever the previous operation left it in
        NRF_RADIO->INTENCLR = 0xffffffff;
        NRF_RADIO->SHORTS   = 0;

        if ( ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) != ( RADIO_STATE_STATE_Disabled << RADIO_STATE_STATE_Pos ) )
        {
            NRF_RADIO->EVENTS_DISABLED = 0;
            NRF_RADIO->TASKS_DISABLE   = 1;
            while ( !NRF_RADIO->EVENTS_DISABLED )
                ;
        }
        NRF_RADIO->EVENTS_DISABLED = 0;

        NRF_RADIO->FREQUENCY    = frequency_from_channel( channel );
        NRF_RADIO->DATAWHITEIV  = channel & 0x3f;
        NRF_RADIO->PACKETPTR    = reinterpret_cast< std::uint32_t >( receive_buffer_ );

        // start receiving when ready, sample RSSI at the address match, stay in RXIDLE after a packet
        NRF_RADIO->SHORTS       = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk;

        NRF_RADIO->EVENTS_ADDRESS   = 0;
        NRF_RADIO->EVENTS_END       = 0;
        NRF_RADIO->INTENSET         = RADIO_INTENSET_END_Msk;

        NRF_TIMER0->EVENTS_COMPARE[ cc_window ] = 0;
        NRF_TIMER0->TASKS_CAPTURE[ cc_now ]     = 1;
        NRF_TIMER0->CC[ cc_window ]             = NRF_TIMER0->CC[ cc_now ] + static_cast< std::uint32_t >( ticks );
        NRF_TIMER0->INTENSET = TIMER_INTENSET_COMPARE1_Msk;

        NRF_RADIO->TASKS_RXEN = 1;
    }

    void platform::on_packet_end()
    {
        NRF_RADIO->EVENTS_END = 0;

        const bool crc_ok = ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk )
            == ( RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos );

        const std::uint32_t first_bit = NRF_TIMER0->CC[ cc_address ] - preamble_and_access_address_ticks;
        const std::size_t   size      = std::min< std::size_t >( receive_buffer_[ 1 ] + 2, max_advertising_pdu_size );

        tester_happened event;
        event.kind   = tester_event::received;
        event.when   = tester_time{ first_bit };
        event.data   = pdu( std::span< const std::uint8_t >( receive_buffer_, size ) );
        event.crc_ok = crc_ok;
        event.rssi   = static_cast< std::uint8_t >( NRF_RADIO->RSSISAMPLE );

        enqueue( event );

        // receive the next packet of the window
        NRF_RADIO->TASKS_START = 1;

        __SEV();
    }

    void platform::on_window_end()
    {
        NRF_TIMER0->EVENTS_COMPARE[ cc_window ] = 0;
        NRF_TIMER0->INTENCLR = TIMER_INTENCLR_COMPARE1_Msk;

        NRF_RADIO->INTENCLR      = RADIO_INTENCLR_END_Msk;
        NRF_RADIO->SHORTS        = 0;
        NRF_RADIO->TASKS_DISABLE = 1;

        tester_happened event{};
        event.kind = tester_event::window_ended;

        enqueue( event );

        __SEV();
    }

    void platform::enqueue( const tester_happened& event )
    {
        if ( event_tail_ - event_head_ == event_ring_size )
            return;

        events_[ event_tail_ % event_ring_size ] = event;
        __DMB();
        event_tail_ = event_tail_ + 1;
    }

    std::optional< tester_happened > platform::next_event()
    {
        if ( event_head_ == event_tail_ )
            return std::nullopt;

        const tester_happened event = events_[ event_head_ % event_ring_size ];
        __DMB();
        event_head_ = event_head_ + 1;

        return event;
    }

    void platform::radio_interrupt()
    {
        if ( instance_ )
            instance_->on_packet_end();
    }

    void platform::timer_interrupt()
    {
        if ( instance_ )
            instance_->on_window_end();
    }
}
}

extern "C" void RADIO_IRQHandler()
{
    bluetoe::test_rig::platform::radio_interrupt();
}

extern "C" void TIMER0_IRQHandler()
{
    bluetoe::test_rig::platform::timer_interrupt();
}
