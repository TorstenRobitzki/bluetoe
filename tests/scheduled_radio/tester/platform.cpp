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

        /*
         * The inter frame space of the Core Specification, which the radio's TIFS keeps
         * between the end of a received PDU and the start of the answer.
         */
        constexpr std::uint32_t inter_frame_space_us = 150;

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
        // a plain receive answers nothing; answer() sets this once the receiver is armed
        answering_ = false;

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

    /*
     * An answer operation is a receive that answers: the first advertising PDU from `target`
     * is met with `response` one inter frame space after it ended. The radio times that itself,
     * by its TIFS and the DISABLED to TXEN short, so the answer's timing owes nothing to
     * the interrupt; the interrupt only decides, at the end of the received packet and
     * before the radio has finished disabling, whether to arm that short. After every
     * packet it does not answer, and after the answer itself, the receiver is re-armed
     * from the DISABLED interrupt, so the operation keeps listening for the rest of its
     * window.
     *
     * The receiver's ramp up is tens of microseconds, so the state and the shorts set
     * after receive() are in place long before a packet could end.
     */
    void platform::answer(
        std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
        const link_layer::device_address& target, const pdu& response )
    {
        receive( channel, phy, ticks );

        target_ = target;
        std::copy( response.data.begin(), response.data.begin() + response.size, response_buffer_ );

        answered_     = false;
        transmitting_ = false;
        answering_    = true;

        // a packet ends the reception, so the radio is disabled for the inter frame space
        // the answer is timed from; nothing is answered until the interrupt arms it
        NRF_RADIO->TIFS      = inter_frame_space_us;
        NRF_RADIO->SHORTS   |= RADIO_SHORTS_END_DISABLE_Msk;
        NRF_RADIO->INTENSET  = RADIO_INTENSET_DISABLED_Msk;
    }

    /*
     * The END of a packet: a reception, or in an answer operation possibly the answer going
     * out. The ADDRESS event fires for a transmitted packet just as for a received one, so
     * the same capture times the answer's first bit.
     */
    void platform::on_packet_end()
    {
        NRF_RADIO->EVENTS_END = 0;

        const std::uint32_t first_bit = NRF_TIMER0->CC[ cc_address ] - preamble_and_access_address_ticks;

        if ( transmitting_ )
        {
            transmitting_ = false;

            // the answer is out; the END to DISABLE short now only closes the transmission,
            // and DISABLED re-arms the receiver, which answers no second time
            NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk | RADIO_SHORTS_END_DISABLE_Msk;

            const std::size_t size = std::min< std::size_t >( response_buffer_[ 1 ] + 2, max_advertising_pdu_size );

            const tester_happened event{
                .kind   = tester_event::transmitted,
                .when   = tester_time{ first_bit },
                .data   = pdu( std::span< const std::uint8_t >( response_buffer_, size ) ),
                .crc_ok = true,
                .rssi   = 0 };

            enqueue( event );
            __SEV();

            return;
        }

        const bool crc_ok = ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk )
            == ( RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos );

        const std::size_t size = std::min< std::size_t >( receive_buffer_[ 1 ] + 2, max_advertising_pdu_size );

        const tester_happened event{
            .kind   = tester_event::received,
            .when   = tester_time{ first_bit },
            .data   = pdu( std::span< const std::uint8_t >( receive_buffer_, size ) ),
            .crc_ok = crc_ok,
            .rssi   = static_cast< std::uint8_t >( NRF_RADIO->RSSISAMPLE ) };

        enqueue( event );

        if ( answering_ )
        {
            // the first PDU from the target is answered: the transmission is armed now,
            // while the radio is still disabling, and its TIFS places the answer one inter
            // frame space after this packet ended. Everything else is re-armed from DISABLED.
            if ( !answered_ && crc_ok && from_target() )
            {
                answered_     = true;
                transmitting_ = true;

                NRF_RADIO->PACKETPTR = reinterpret_cast< std::uint32_t >( response_buffer_ );
                NRF_RADIO->SHORTS    = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;
            }
        }
        else
        {
            // receive the next packet of the window
            NRF_RADIO->TASKS_START = 1;
        }

        __SEV();
    }

    /*
     * Whether the received advertising PDU is from the target: its AdvA, the six
     * bytes after the two byte header, and the address's kind by the header's TxAdd bit.
     */
    bool platform::from_target() const
    {
        constexpr std::uint8_t tx_add_mask = 0x40;

        const bool is_random = receive_buffer_[ 0 ] & tx_add_mask;

        return is_random == target_.is_random()
            && std::equal( target_.begin(), target_.end(), &receive_buffer_[ 2 ] );
    }

    /*
     * In an answer operation the radio disables itself after every packet, by the END to
     * DISABLE short. After a packet that was not answered, and after the answer itself, the
     * receiver is re-armed here, so the operation keeps listening; while the answer is armed
     * the short from DISABLED to TXEN is doing the transmitting, and nothing is re-armed.
     */
    void platform::on_radio_disabled()
    {
        NRF_RADIO->EVENTS_DISABLED = 0;

        if ( !answering_ || transmitting_ )
            return;

        NRF_RADIO->PACKETPTR  = reinterpret_cast< std::uint32_t >( receive_buffer_ );
        NRF_RADIO->TASKS_RXEN = 1;
    }

    void platform::on_window_end()
    {
        NRF_TIMER0->EVENTS_COMPARE[ cc_window ] = 0;
        NRF_TIMER0->INTENCLR = TIMER_INTENCLR_COMPARE1_Msk;

        // an answer operation re-arms the receiver from DISABLED; not the disable that ends
        // the window
        answering_    = false;
        transmitting_ = false;

        NRF_RADIO->INTENCLR      = RADIO_INTENCLR_END_Msk | RADIO_INTENCLR_DISABLED_Msk;
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

    /*
     * END before DISABLED: with the END to DISABLE short both can be pending together,
     * and the decision made at END is what the handling of DISABLED then respects.
     */
    void platform::radio_interrupt()
    {
        if ( !instance_ )
            return;

        if ( NRF_RADIO->EVENTS_END )
            instance_->on_packet_end();

        if ( NRF_RADIO->EVENTS_DISABLED )
            instance_->on_radio_disabled();
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
