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
        constexpr std::size_t   cc_answer           = 3;    // the transmitter of an answer ramps up

        /*
         * One PPI channel captures the timer at the RADIO ADDRESS event, another starts the
         * transmitter of an answer at its compare.
         */
        constexpr std::size_t   ppi_address_capture = 0;
        constexpr std::size_t   ppi_answer_txen     = 1;

        constexpr std::uint32_t ticks_per_us        = 16;

        /*
         * The timing of a packet on each PHY. The RADIO ADDRESS event fires
         * once the preamble and the access address are on air, so the first bit of the packet
         * was one preamble and one access address earlier: 8 + 32 bits at 1 Mbit, 40 µs, and
         * 16 + 32 bits at 2 Mbit, 24 µs. The value only shifts the origin, which cancels in the
         * interval a test compares, but it is the true offset for an absolute measurement such
         * as T_IFS.
         *
         * A received packet's ADDRESS event comes later than a transmitted one's, by the
         * receiver's address detection. Measured against the device under test's scan
         * response, which its radio's TIFS places 150 µs after the request: 172 ticks at
         * 1 Mbit. At 2 Mbit the same against the device's reply in a connection event, taken
         * to start 150 µs after the tester's PDU ended: 96 ticks.
         */
        struct phy_timing
        {
            std::uint32_t preamble_and_access_address_ticks;
            std::uint32_t address_detection_ticks;
            std::uint32_t ticks_per_byte;
        };

        constexpr phy_timing le_1m_timing{ 40 * ticks_per_us, 172, 8 * ticks_per_us };
        constexpr phy_timing le_2m_timing{ 24 * ticks_per_us, 96, 4 * ticks_per_us };

        const phy_timing& timing( bool two_mbit )
        {
            return two_mbit ? le_2m_timing : le_1m_timing;
        }

        /*
         * The inter frame space of the Core Specification, from the end of a received PDU
         * to the first bit of the answer. The radio's TIFS does not keep it with fast ramp
         * up, so the timer starts the transmitter one ramp up earlier.
         */
        constexpr std::uint32_t inter_frame_space_us = 150;
        constexpr std::uint32_t fast_ramp_up_us      = 40;

        // the ticks from the first bit of a packet to its last: preamble, access address,
        // header, payload and CRC
        std::uint32_t air_ticks( const phy_timing& timing, std::uint32_t payload_size )
        {
            return timing.preamble_and_access_address_ticks + ( 2 + payload_size + 3 ) * timing.ticks_per_byte;
        }

        // an answer operation disables the receiver after every packet, so that the
        // transmitter can be started from there
        constexpr std::uint32_t shorts_answering =
            RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk | RADIO_SHORTS_END_DISABLE_Msk;

        bool radio_disabled()
        {
            return ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) == ( RADIO_STATE_STATE_Disabled << RADIO_STATE_STATE_Pos );
        }

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
        set_access_address_and_crc_init( advertising_access_address, advertising_crc_init );

        NRF_PPI->CH[ ppi_address_capture ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_ADDRESS );
        NRF_PPI->CH[ ppi_address_capture ].TEP = reinterpret_cast< std::uint32_t >( &NRF_TIMER0->TASKS_CAPTURE[ cc_address ] );
        NRF_PPI->CHENSET = 1u << ppi_address_capture;

        NRF_PPI->CH[ ppi_answer_txen ].EEP = reinterpret_cast< std::uint32_t >( &NRF_TIMER0->EVENTS_COMPARE[ cc_answer ] );
        NRF_PPI->CH[ ppi_answer_txen ].TEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->TASKS_TXEN );

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
        access_address_ = access_address;
        crc_init_       = crc_init;

        NRF_RADIO->BASE0    = access_address << 8;
        NRF_RADIO->PREFIX0  = access_address >> 24;
        NRF_RADIO->CRCINIT  = crc_init;
    }

    /*
     * No interrupt of the radio or the window is enabled or pending afterwards, so no event is
     * queued until the next operation arms them again. Both are silenced with interrupts
     * off, as an interrupt already on its way would otherwise queue an event after this.
     */
    void platform::stop()
    {
        __disable_irq();

        NRF_TIMER0->INTENCLR                    = TIMER_INTENCLR_COMPARE1_Msk;
        NRF_TIMER0->EVENTS_COMPARE[ cc_window ] = 0;
        NRF_RADIO->INTENCLR                     = 0xffffffff;
        NRF_RADIO->SHORTS                       = 0;
        NRF_PPI->CHENCLR                        = 1u << ppi_answer_txen;
        NVIC_ClearPendingIRQ( TIMER0_IRQn );
        NVIC_ClearPendingIRQ( RADIO_IRQn );

        answering_    = false;
        transmitting_ = false;
        connecting_   = false;

        // a PDU with an invalid CRC may have been armed but never sent
        NRF_RADIO->CRCINIT = crc_init_;

        __enable_irq();

        if ( !radio_disabled() )
        {
            NRF_RADIO->EVENTS_DISABLED = 0;
            NRF_RADIO->TASKS_DISABLE   = 1;
            while ( !NRF_RADIO->EVENTS_DISABLED )
                ;
        }
        NRF_RADIO->EVENTS_DISABLED = 0;
    }

    /*
     * The radio compares the TxAdd bit and the six bytes after the header, an advertising
     * PDU's AdvA, with the enabled slots: the first four bytes in DAB, the last two in DAP.
     */
    void platform::accept_advertiser( std::uint32_t slot, const link_layer::device_address& address )
    {
        assert( slot < 8 );

        const std::uint8_t* bytes = address.begin();

        NRF_RADIO->DAB[ slot ] = bytes[ 0 ] | ( bytes[ 1 ] << 8 ) | ( bytes[ 2 ] << 16 ) | ( std::uint32_t( bytes[ 3 ] ) << 24 );
        NRF_RADIO->DAP[ slot ] = bytes[ 4 ] | ( bytes[ 5 ] << 8 );

        NRF_RADIO->DACNF = ( NRF_RADIO->DACNF & ~( ( 1u << slot ) | ( 1u << ( RADIO_DACNF_TXADD0_Pos + slot ) ) ) )
            | ( 1u << slot )
            | ( address.is_random() ? 1u << ( RADIO_DACNF_TXADD0_Pos + slot ) : 0 );

        matching_advertisers_ = true;
    }

    /*
     * A packet from an advertiser outside the slots, abandoned once its address is known: the
     * receiver stops and starts again without ramping up, so that the device under test is
     * not missed while the rest of the stranger's packet is on air. Only a reception is
     * stopped; after an END the radio may be disabling or transmitting an answer already.
     */
    void platform::on_device_address_miss()
    {
        NRF_RADIO->EVENTS_DEVMISS = 0;

        if ( ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) != ( RADIO_STATE_STATE_Rx << RADIO_STATE_STATE_Pos ) )
            return;

        NRF_RADIO->TASKS_STOP  = 1;
        NRF_RADIO->TASKS_START = 1;
    }

    /*
     * Listen on the channel until the window is over, restarting the receiver after every
     * packet so that a whole window of PDUs is caught.
     */
    void platform::receive( std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
        std::uint32_t operation_id )
    {
        prepare( channel, phy, ticks, operation_id );

        // start receiving when ready, sample RSSI at the address match, stay in RXIDLE after a packet
        NRF_RADIO->SHORTS       = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk;
        NRF_RADIO->TASKS_RXEN   = 1;
    }

    /*
     * What every operation begins with: the radio stopped and set to the channel and the
     * PHY, its interrupts on, and the window running.
     */
    void platform::prepare( std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
        std::uint32_t operation_id )
    {
        stop();

        // nothing is queued while stopped, so every event from here on is this operation's
        operation_id_ = operation_id;

        two_mbit_ = phy == link_layer::phy_ll_encoding::le_2m_phy;

        // the preamble is one byte at 1 Mbit and two at 2 Mbit
        NRF_RADIO->MODE  = ( two_mbit_ ? RADIO_MODE_MODE_Ble_2Mbit : RADIO_MODE_MODE_Ble_1Mbit ) << RADIO_MODE_MODE_Pos;
        NRF_RADIO->PCNF0 = ( NRF_RADIO->PCNF0 & ~RADIO_PCNF0_PLEN_Msk )
            | ( ( two_mbit_ ? RADIO_PCNF0_PLEN_16bit : RADIO_PCNF0_PLEN_8bit ) << RADIO_PCNF0_PLEN_Pos );

        NRF_RADIO->FREQUENCY    = frequency_from_channel( channel );
        NRF_RADIO->DATAWHITEIV  = channel & 0x3f;
        NRF_RADIO->PACKETPTR    = reinterpret_cast< std::uint32_t >( receive_buffer_ );

        // only a PDU on the advertising access address has an advertiser's address to match
        const bool match_advertisers = matching_advertisers_ && access_address_ == advertising_access_address;

        NRF_RADIO->EVENTS_ADDRESS   = 0;
        NRF_RADIO->EVENTS_END       = 0;
        NRF_RADIO->EVENTS_DEVMISS   = 0;
        NRF_RADIO->INTENSET         = RADIO_INTENSET_END_Msk | ( match_advertisers ? RADIO_INTENSET_DEVMISS_Msk : 0 );

        NRF_TIMER0->EVENTS_COMPARE[ cc_window ] = 0;
        NRF_TIMER0->TASKS_CAPTURE[ cc_now ]     = 1;
        NRF_TIMER0->CC[ cc_window ]             = NRF_TIMER0->CC[ cc_now ] + static_cast< std::uint32_t >( ticks );
        NRF_TIMER0->INTENSET = TIMER_INTENSET_COMPARE1_Msk;
    }

    /*
     * A connection event as its central: the first PDU goes out at `at` and every further one
     * `t_ifs` after the reply to the one before ended, each started by the timer through PPI
     * like an answer, so that the timing owes nothing to an interrupt. The END of a PDU
     * disables the radio and DISABLED starts the receiver for the reply by a short, which the
     * transmitter's READY adds only once the transmission runs: added while the radio is still
     * disabling after a reply, it would start the receiver instead of waiting for the compare.
     * The END of a reply arms the next PDU, or leaves the radio disabled after the last. A PDU
     * sent with an invalid CRC is sent with another CRC init, which its END restores before the
     * receiver for the reply starts.
     */
    bool platform::connection_event( std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
        std::uint32_t at, const std::array< pdu, max_event_pdus >& pdus, std::uint32_t count, std::uint32_t t_ifs,
        std::uint32_t crc_errors, std::uint32_t operation_id )
    {
        prepare( channel, phy, ticks, operation_id );

        for ( std::uint32_t index = 0; index != count; ++index )
            std::copy( pdus[ index ].data.begin(), pdus[ index ].data.begin() + pdus[ index ].size, event_pdus_[ index ] );

        event_count_      = count;
        event_next_       = 0;
        event_t_ifs_      = t_ifs;
        event_crc_errors_ = crc_errors;
        connecting_       = true;

        NRF_RADIO->EVENTS_READY    = 0;
        NRF_RADIO->EVENTS_DISABLED = 0;
        NRF_RADIO->INTENSET        = RADIO_INTENSET_READY_Msk | RADIO_INTENSET_DISABLED_Msk;

        if ( arm_event_transmission( at ) )
            return true;

        stop();

        return false;
    }

    /*
     * The event's next PDU, with its first bit on air at `at`: the compare starts the
     * transmitter one ramp up earlier, and the compare is set before PPI forwards it. False if
     * the timer is past it then; a transmitter the compare started meanwhile is cancelled.
     */
    bool platform::arm_event_transmission( std::uint32_t at )
    {
        const std::uint32_t start = at - fast_ramp_up_us * ticks_per_us;

        transmitting_        = true;
        NRF_RADIO->PACKETPTR = reinterpret_cast< std::uint32_t >( event_pdus_[ event_next_ ] );
        NRF_RADIO->SHORTS    = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
        NRF_RADIO->CRCINIT   = crc_error( event_next_ ) ? crc_init_ ^ 1 : crc_init_;

        NRF_TIMER0->EVENTS_COMPARE[ cc_answer ] = 0;
        NRF_TIMER0->CC[ cc_answer ]             = start;
        NRF_PPI->CHENSET                        = 1u << ppi_answer_txen;

        // ahead means less than half the clock's range ahead of now
        NRF_TIMER0->TASKS_CAPTURE[ cc_now ] = 1;

        if ( start - NRF_TIMER0->CC[ cc_now ] < 0x80000000u )
            return true;

        transmitting_            = false;
        NRF_PPI->CHENCLR         = 1u << ppi_answer_txen;
        NRF_RADIO->TASKS_DISABLE = 1;

        return false;
    }

    bool platform::crc_error( std::uint32_t pdu_index ) const
    {
        return event_crc_errors_ & ( 1u << pdu_index );
    }

    /*
     * The transmitter of an event's PDU is ready: the reply is received right after it, by
     * the short from DISABLED. A receiver's READY needs nothing.
     */
    void platform::on_radio_ready()
    {
        NRF_RADIO->EVENTS_READY = 0;

        if ( !connecting_ || !transmitting_ )
            return;

        NRF_PPI->CHENCLR   = 1u << ppi_answer_txen;
        NRF_RADIO->SHORTS  = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_RXEN_Msk;
    }

    /*
     * The END of an event's PDU, reported with its first bit, or of the reply to it, after
     * which the next PDU is armed one inter frame space after the reply ended. A reply with a
     * CRC error is answered all the same: the test scripted the flow, and the device's reply
     * is what it reports on.
     */
    void platform::on_event_packet_end()
    {
        const std::uint32_t address = NRF_TIMER0->CC[ cc_address ];

        if ( transmitting_ )
        {
            transmitting_        = false;
            NRF_RADIO->PACKETPTR = reinterpret_cast< std::uint32_t >( receive_buffer_ );
            NRF_RADIO->CRCINIT   = crc_init_;

            const std::uint8_t* sent   = event_pdus_[ event_next_ ];
            const std::size_t   size   = std::min< std::size_t >( sent[ 1 ] + 2, max_advertising_pdu_size );
            const bool          crc_ok = !crc_error( event_next_ );

            event_next_ = event_next_ + 1;

            const tester_happened event{
                .kind   = tester_event::transmitted,
                .when   = tester_time{ address - timing( two_mbit_ ).preamble_and_access_address_ticks },
                .data   = pdu( std::span< const std::uint8_t >( sent, size ) ),
                .crc_ok = crc_ok,
                .rssi   = 0 };

            enqueue( event );
            __SEV();

            return;
        }

        // the address match still compares a data channel PDU; its miss must not restart the
        // receiver later, as it would for a stranger's advertising
        NRF_RADIO->EVENTS_DEVMISS = 0;

        const std::uint32_t first_bit = address - timing( two_mbit_ ).preamble_and_access_address_ticks - timing( two_mbit_ ).address_detection_ticks;

        const bool crc_ok = ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk )
            == ( RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos );

        // decided first, as the next PDU's deadline runs from the end of this one
        if ( event_next_ != event_count_ )
            arm_event_transmission( first_bit + air_ticks( timing( two_mbit_ ), receive_buffer_[ 1 ] ) + event_t_ifs_ );

        const std::size_t size = std::min< std::size_t >( receive_buffer_[ 1 ] + 2, max_advertising_pdu_size );

        const tester_happened event{
            .kind   = tester_event::received,
            .when   = tester_time{ first_bit },
            .data   = pdu( std::span< const std::uint8_t >( receive_buffer_, size ) ),
            .crc_ok = crc_ok,
            .rssi   = static_cast< std::uint8_t >( NRF_RADIO->RSSISAMPLE ) };

        enqueue( event );

        __SEV();
    }

    /*
     * An answer operation is a receive that answers: the first advertising PDU from `target`
     * is met with `response` one inter frame space after it ended. Every packet disables the
     * receiver; the END interrupt, with more than a hundred microseconds to spare, sets a
     * timer compare that starts the transmitter by PPI, so that the answer's timing owes
     * nothing to the interrupt. After every packet it does not answer, and after the answer
     * itself, the receiver is re-armed from DISABLED, so the operation keeps listening for
     * the rest of its window.
     *
     * The receiver's ramp up is tens of microseconds, so the state and the shorts set
     * after receive() are in place long before a packet could end.
     */
    void platform::answer(
        std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
        const link_layer::device_address& target, const pdu& response, std::uint32_t operation_id )
    {
        receive( channel, phy, ticks, operation_id );

        target_ = target;
        std::copy( response.data.begin(), response.data.begin() + response.size, response_buffer_ );

        answered_     = false;
        transmitting_ = false;
        answering_    = true;

        NRF_RADIO->SHORTS    = shorts_answering;
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

        if ( connecting_ )
        {
            on_event_packet_end();

            return;
        }

        const std::uint32_t address = NRF_TIMER0->CC[ cc_address ];

        if ( transmitting_ )
        {
            transmitting_ = false;

            // the answer is out; DISABLED re-arms the receiver, which answers no second time
            NRF_PPI->CHENCLR = 1u << ppi_answer_txen;

            const std::size_t size = std::min< std::size_t >( response_buffer_[ 1 ] + 2, max_advertising_pdu_size );

            const tester_happened event{
                .kind   = tester_event::transmitted,
                .when   = tester_time{ address - timing( two_mbit_ ).preamble_and_access_address_ticks },
                .data   = pdu( std::span< const std::uint8_t >( response_buffer_, size ) ),
                .crc_ok = true,
                .rssi   = 0 };

            enqueue( event );
            __SEV();

            return;
        }

        const std::uint32_t first_bit = address - timing( two_mbit_ ).preamble_and_access_address_ticks - timing( two_mbit_ ).address_detection_ticks;

        const bool crc_ok = ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk )
            == ( RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos );

        if ( answering_ )
        {
            // decided first, as the answer's deadline runs from the end of this packet
            if ( !answered_ && crc_ok && from_target() )
                arm_answer( first_bit );
        }
        else
        {
            // receive the next packet of the window
            NRF_RADIO->TASKS_START = 1;
        }

        const std::size_t size = std::min< std::size_t >( receive_buffer_[ 1 ] + 2, max_advertising_pdu_size );

        const tester_happened event{
            .kind   = tester_event::received,
            .when   = tester_time{ first_bit },
            .data   = pdu( std::span< const std::uint8_t >( receive_buffer_, size ) ),
            .crc_ok = crc_ok,
            .rssi   = static_cast< std::uint8_t >( NRF_RADIO->RSSISAMPLE ) };

        enqueue( event );

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
     * Starts the transmitter of the answer by the timer, one ramp up before the inter frame
     * space after the received packet ended, `first_bit` being that packet's first bit. The
     * compare is set before PPI forwards it, and PPI forwards only a compare that happens
     * after it was enabled: if the timer is already past it then, the answer is given up
     * rather than sent late, and the packet is treated as one not answered.
     */
    void platform::arm_answer( std::uint32_t first_bit )
    {
        const std::uint32_t start = air_ticks( timing( two_mbit_ ), receive_buffer_[ 1 ] ) + ( inter_frame_space_us - fast_ramp_up_us ) * ticks_per_us;

        NRF_RADIO->PACKETPTR = reinterpret_cast< std::uint32_t >( response_buffer_ );

        NRF_TIMER0->EVENTS_COMPARE[ cc_answer ] = 0;
        NRF_TIMER0->CC[ cc_answer ]             = first_bit + start;
        NRF_PPI->CHENSET                        = 1u << ppi_answer_txen;

        NRF_TIMER0->TASKS_CAPTURE[ cc_now ] = 1;

        if ( NRF_TIMER0->CC[ cc_now ] - first_bit < start )
        {
            answered_     = true;
            transmitting_ = true;

            return;
        }

        // too late; a transmitter the compare started meanwhile is cancelled
        NRF_PPI->CHENCLR         = 1u << ppi_answer_txen;
        NRF_RADIO->PACKETPTR     = reinterpret_cast< std::uint32_t >( receive_buffer_ );
        NRF_RADIO->TASKS_DISABLE = 1;
    }

    /*
     * In an answer operation the radio disables itself after every packet. While an answer
     * is armed, the timer starts the transmitter from here. Otherwise the receiver is
     * re-armed once the radio is really disabled: a cancelled transmitter disables a second
     * time.
     */
    void platform::on_radio_disabled()
    {
        NRF_RADIO->EVENTS_DISABLED = 0;

        // after one of its PDUs the radio is ramping up the receiver for the reply already,
        // by the short; after the reply it stays disabled for the next PDU's compare
        if ( connecting_ )
        {
            if ( !transmitting_ )
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk | RADIO_SHORTS_END_DISABLE_Msk;

            return;
        }

        if ( !answering_ || transmitting_ || !radio_disabled() )
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
        connecting_   = false;

        NRF_RADIO->CRCINIT       = crc_init_;
        NRF_RADIO->INTENCLR      = RADIO_INTENCLR_END_Msk | RADIO_INTENCLR_READY_Msk | RADIO_INTENCLR_DISABLED_Msk;
        NRF_RADIO->SHORTS        = 0;
        NRF_PPI->CHENCLR         = 1u << ppi_answer_txen;
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

        events_[ event_tail_ % event_ring_size ]              = event;
        events_[ event_tail_ % event_ring_size ].operation_id = operation_id_;
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

        if ( NRF_RADIO->EVENTS_READY )
            instance_->on_radio_ready();

        if ( NRF_RADIO->EVENTS_END )
            instance_->on_packet_end();
        else if ( NRF_RADIO->EVENTS_DEVMISS )
            instance_->on_device_address_miss();

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
