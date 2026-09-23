#ifndef BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_RADIO_BASE_HPP
#define BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_RADIO_BASE_HPP

/**
 * @file nrf52_radio_base.hpp
 *
 * The definitions of radio_base_t, which nrf52_radio.hpp includes at its end; consumers
 * include nrf52_radio.hpp.
 */

#include <bluetoe/nrf52_radio.hpp>
#include <bluetoe/ll_constants.hpp>

#include <nrf.h>

#include <algorithm>
#include <cassert>

namespace bluetoe
{
    namespace nrf52_details
    {
        inline radio_lock_guard::radio_lock_guard()
            : primask_( __get_PRIMASK() )
        {
            __disable_irq();
        }

        inline radio_lock_guard::~radio_lock_guard()
        {
            __set_PRIMASK( primask_ );
        }

        /*
         * The radio's constants and the helpers on the hardware; inline, as the binding's own
         * source files include the radio without instantiating it.
         */
        namespace {

            /*
             * Both timers count microseconds: the 16 MHz peripheral clock through a
             * prescaler of 2^4.
             */
            constexpr std::uint32_t prescaler_for_1us = 4;

            /*
             * TIMER0 belongs to the radio: compare 0 starts a transmission, compare 1 ends
             * the receive window, capture 2 takes the end of a packet, capture 3 reads the
             * time. It runs during a radio event only; see the time base in nrf52_radio.hpp.
             */
            constexpr std::size_t cc_start          = 0;
            constexpr std::size_t cc_window_end     = 1;
            constexpr std::size_t cc_packet_end     = 2;
            constexpr std::size_t cc_now            = 3;

            /*
             * RTC0 on the sleep clock: compare 0 starts the high frequency crystal, compare 1
             * starts TIMER0, both through PPI; compare 2 is the user timer, through the
             * interrupt. Its 24 bits overflow every 512 s, counted in the interrupt.
             */
            constexpr std::size_t rtc_cc_hfxo       = 0;
            constexpr std::size_t rtc_cc_timer      = 1;
            constexpr std::size_t rtc_cc_user_timer = 2;
            constexpr std::size_t rtc_bits          = 24;

            /*
             * A tick of the sleep clock is 1000000 / 32768 µs = 15625 / 512 µs, exactly; a
             * radio event has to be placed to the tick at least this far ahead for the RTC
             * to match a compare it is given now.
             */
            constexpr std::uint32_t us_per_tick_numerator   = 15625;
            constexpr std::uint32_t us_per_tick_denominator = 512;
            constexpr std::uint32_t tick_us                 = 31;
            constexpr std::uint32_t rtc_compare_lead_ticks  = 2;

            /*
             * Microseconds ahead into ticks ahead, rounded down or up, without a 64 bit
             * division: the quotient by the numerator first, then the remainder, which
             * stays small.
             */
            inline std::uint32_t ticks_of( std::uint32_t microseconds, bool round_up )
            {
                const std::uint32_t whole = microseconds / us_per_tick_numerator;
                const std::uint32_t rest  = microseconds % us_per_tick_numerator;
                const std::uint32_t part  = rest * us_per_tick_denominator + ( round_up ? us_per_tick_numerator - 1 : 0 );

                return whole * us_per_tick_denominator + part / us_per_tick_numerator;
            }

            /*
             * The pre-programmed PPI channels of the nRF52, and two free channels for the
             * RTC's compares.
             */
            constexpr std::uint32_t ppi_compare0_txen       = 1u << 20;
            constexpr std::uint32_t ppi_compare0_rxen       = 1u << 21;
            constexpr std::uint32_t ppi_compare1_disable    = 1u << 22;
            constexpr std::uint32_t ppi_end_capture2        = 1u << 27;
            // pre-programmed: the address of a packet starts the CCM; the encryption arms it
            constexpr std::uint32_t ppi_address_ccm_crypt   = 1u << 25;

            constexpr std::size_t   ppi_rtc_hfxo_channel    = 1;
            constexpr std::size_t   ppi_rtc_timer_channel   = 2;
            constexpr std::uint32_t ppi_rtc_hfxo            = 1u << ppi_rtc_hfxo_channel;
            constexpr std::uint32_t ppi_rtc_timer           = 1u << ppi_rtc_timer_channel;

            // a PDU in memory: S0, the length and the spare byte the CCM needs (encrypted_pdu_layout)
            constexpr std::size_t   memory_header_size      = 3;

            /*
             * The radio ramps up with the default ramp up, as TIFS is only kept with that
             * one; about 140 µs, as measured with the tester.
             */
            constexpr std::uint32_t ramp_up_us = 140;

            /*
             * How far ahead a request has to be for the radio to be set up in time: the
             * ramp up, and a margin for the setup itself. The setup runs with interrupts
             * off, from the reading of the clock the request is checked against to the
             * arming of the start, so nothing can use that margin up in between and the
             * start can never slip past before the channel that forwards it is enabled.
             */
            constexpr std::uint32_t setup_us = 60;

            /*
             * With the crystal to start before the event, the ticks the RTC needs to match its
             * compares, and the remainder TIMER0 has to have room for.
             */
            template < radio_configuration Configuration >
            constexpr std::uint32_t earliest_us =
                Configuration.hfxo_startup_us + ramp_up_us + ( rtc_compare_lead_ticks + 2 ) * tick_us + setup_us;

            template < radio_configuration Configuration >
            constexpr std::uint32_t hfxo_startup_ticks =
                ( Configuration.hfxo_startup_us * us_per_tick_denominator + us_per_tick_numerator - 1 ) / us_per_tick_numerator + 1;

            /*
             * Interrupts off for the few microseconds of that setup, the mask restored
             * afterwards so that a caller already in a critical section stays in one.
             */
            using interrupts_off = radio_lock_guard;

            inline bool radio_disabled()
            {
                return ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) == ( RADIO_STATE_STATE_Disabled << RADIO_STATE_STATE_Pos );
            }

            // whether the packet just received had a valid CRC
            inline bool received_crc_ok()
            {
                return ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk )
                    == ( RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos );
            }

            /*
             * The receive window after an advertising PDU: the inter frame space, the
             * longest legacy response (CONNECT_IND: preamble, access address, header, 34
             * bytes, CRC, at 1 Mbit) and a margin for the receiver's ramp up.
             */
            using link_layer::inter_frame_space_us;

            constexpr std::uint32_t longest_response_us     = ( 1 + 4 + 2 + 34 + 3 ) * 8;
            constexpr std::uint32_t response_window_us      = inter_frame_space_us + longest_response_us + 50;

            /*
             * The timing of a packet on each PHY: its preamble and access address, 8 + 32 bits
             * at 1 Mbit and 16 + 32 bits at 2 Mbit, a byte, and the receiver's address
             * detection, which the tester measured at 10.75 µs at 1 Mbit and at 6 µs at 2 Mbit
             * on this radio.
             */
            struct phy_timing
            {
                std::uint32_t preamble_and_access_address_us;
                std::uint32_t byte_us;
                std::uint32_t address_detection_us;
            };

            constexpr phy_timing le_1m_timing{ ( 1 + 4 ) * 8, 8, 11 };
            constexpr phy_timing le_2m_timing{ ( 2 + 4 ) * 4, 4, 6 };

            inline const phy_timing& timing( bool two_mbit )
            {
                return two_mbit ? le_2m_timing : le_1m_timing;
            }

            /*
             * How long a connection event's receive window stays open after `end`, so that a
             * packet whose first bit was on air by `end` is received: its preamble and access
             * address, the receiver's address detection, and the time the address interrupt
             * takes to stop the window's compare. A packet that begins a few microseconds after
             * `end` may be received too.
             */
            constexpr std::uint32_t address_interrupt_us = 5;

            inline std::uint32_t address_of_a_packet_at_end_us( const phy_timing& timing )
            {
                return timing.preamble_and_access_address_us + timing.address_detection_us + address_interrupt_us;
            }

            /*
             * The receive window after an answer in a connection event: the inter frame space,
             * the preamble and access address of the next PDU, and a margin. Its address event
             * ends the window.
             */
            inline std::uint32_t connection_answer_window_us( const phy_timing& timing )
            {
                return inter_frame_space_us + timing.preamble_and_access_address_us + 50;
            }

            /*
             * The header bits of a data channel PDU the radio reads.
             */
            constexpr std::uint8_t  nesn_mask                   = 0x04;
            constexpr std::uint8_t  sn_mask                     = 0x08;
            constexpr std::uint8_t  md_mask                     = 0x10;

            using link_layer::advertising_access_address;
            using link_layer::advertising_crc_init;

            /*
             * The TxAdd bit of an advertising channel PDU header: set when the sender's
             * address, the first address field of the payload, is random.
             */
            constexpr std::uint8_t  tx_add_mask                 = 0x40;

            /*
             * The RxAdd bit: set when the address the PDU is addressed to, its second
             * address field, is random.
             */
            constexpr std::uint8_t  rx_add_mask                 = 0x80;

            /*
             * An advertising channel PDU carries its type in the low four bits of the
             * header. Only an advertisement that can be scanned is answered, and only a
             * scan request answers it; its payload is the scanner's address followed by
             * the advertiser's, which is the one it is addressed to.
             */
            constexpr std::uint8_t  pdu_type_mask                = 0x0f;
            constexpr std::uint8_t  adv_ind_type                 = 0x00;
            constexpr std::uint8_t  adv_direct_ind_type          = 0x01;
            constexpr std::uint8_t  adv_scan_ind_type            = 0x06;
            constexpr std::uint8_t  scan_request_type            = 0x03;
            constexpr std::uint8_t  connect_request_type         = 0x05;
            constexpr std::uint8_t  scan_request_payload_size    = 12;
            constexpr std::uint8_t  connect_request_payload_size = 34;
            constexpr std::size_t   address_size                 = 6;
            constexpr std::size_t   addressed_to_offset          = memory_header_size + address_size;

            inline bool is_scannable( std::uint8_t header )
            {
                const std::uint8_t type = header & pdu_type_mask;

                return type == adv_ind_type || type == adv_scan_ind_type;
            }

            inline bool is_connectable( std::uint8_t header )
            {
                const std::uint8_t type = header & pdu_type_mask;

                return type == adv_ind_type || type == adv_direct_ind_type;
            }

            /*
             * The Core Specification's channel to frequency mapping, in MHz above 2400.
             */
            inline std::uint32_t frequency_from_channel( std::uint32_t channel )
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

            /*
             * Time on air of a legacy PDU: preamble, access address, header, payload and CRC.
             */
            inline std::uint32_t air_time_us( const phy_timing& timing, std::uint32_t payload_size )
            {
                return timing.preamble_and_access_address_us + ( 2 + payload_size + 3 ) * timing.byte_us;
            }

            // the preamble is one byte at 1 Mbit and two at 2 Mbit
            inline void configure_phy( bool two_mbit )
            {
                NRF_RADIO->MODE  = ( two_mbit ? RADIO_MODE_MODE_Ble_2Mbit : RADIO_MODE_MODE_Ble_1Mbit ) << RADIO_MODE_MODE_Pos;
                NRF_RADIO->PCNF0 = ( NRF_RADIO->PCNF0 & ~RADIO_PCNF0_PLEN_Msk )
                    | ( ( two_mbit ? RADIO_PCNF0_PLEN_16bit : RADIO_PCNF0_PLEN_8bit ) << RADIO_PCNF0_PLEN_Pos );
            }

            inline void configure_timer( NRF_TIMER_Type& timer )
            {
                timer.TASKS_STOP    = 1;
                timer.TASKS_CLEAR   = 1;
                timer.MODE          = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
                timer.BITMODE       = TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos;
                timer.PRESCALER     = prescaler_for_1us;
                timer.INTENCLR      = 0xffffffff;

                for ( auto& compare : timer.EVENTS_COMPARE )
                    compare = 0;
            }

            /*
             * The RTC at the sleep clock's rate; its two compares reach the crystal and
             * TIMER0 through PPI, so their events are routed, and the overflow and the
             * user timer through the interrupt. It starts once the sleep clock runs.
             */
            inline void configure_rtc()
            {
                NRF_RTC0->TASKS_STOP    = 1;
                NRF_RTC0->TASKS_CLEAR   = 1;
                NRF_RTC0->PRESCALER     = 0;
                NRF_RTC0->EVTENCLR      = 0xffffffff;
                NRF_RTC0->INTENCLR      = 0xffffffff;
                NRF_RTC0->EVTENSET      = RTC_EVTENSET_COMPARE0_Msk | RTC_EVTENSET_COMPARE1_Msk;
                NRF_RTC0->INTENSET      = RTC_INTENSET_OVRFLW_Msk;

                for ( auto& compare : NRF_RTC0->EVENTS_COMPARE )
                    compare = 0;

                NRF_RTC0->EVENTS_OVRFLW = 0;

                NRF_PPI->CH[ ppi_rtc_hfxo_channel ].EEP  = reinterpret_cast< std::uint32_t >( &NRF_RTC0->EVENTS_COMPARE[ rtc_cc_hfxo ] );
                NRF_PPI->CH[ ppi_rtc_hfxo_channel ].TEP  = reinterpret_cast< std::uint32_t >( &NRF_CLOCK->TASKS_HFCLKSTART );
                NRF_PPI->CH[ ppi_rtc_timer_channel ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RTC0->EVENTS_COMPARE[ rtc_cc_timer ] );
                NRF_PPI->CH[ ppi_rtc_timer_channel ].TEP = reinterpret_cast< std::uint32_t >( &NRF_TIMER0->TASKS_START );
            }

            inline void configure_radio()
            {
                NRF_RADIO->MODE     = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;
                NRF_RADIO->MODECNF0 =
                      ( RADIO_MODECNF0_RU_Default << RADIO_MODECNF0_RU_Pos )
                    | ( RADIO_MODECNF0_DTX_Center << RADIO_MODECNF0_DTX_Pos );

                // a legacy PDU: one byte of flags, one byte of length, eight bit preamble
                NRF_RADIO->PCNF0 =
                      ( 1 << RADIO_PCNF0_S0LEN_Pos )
                    | ( 8 << RADIO_PCNF0_LFLEN_Pos )
                    | ( 0 << RADIO_PCNF0_S1LEN_Pos )
                    | ( RADIO_PCNF0_S1INCL_Include << RADIO_PCNF0_S1INCL_Pos )
                    | ( RADIO_PCNF0_PLEN_8bit << RADIO_PCNF0_PLEN_Pos );

                NRF_RADIO->PCNF1 =
                      ( RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos )
                    | ( RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos )
                    | ( 3 << RADIO_PCNF1_BALEN_Pos )
                    | ( 0 << RADIO_PCNF1_STATLEN_Pos );

                NRF_RADIO->TXADDRESS    = 0;
                NRF_RADIO->RXADDRESSES  = 1 << 0;

                NRF_RADIO->CRCCNF =
                      ( RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos )
                    | ( RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos );
                // x^24 + x^10 + x^9 + x^6 + x^4 + x^3 + x + 1
                NRF_RADIO->CRCPOLY  = 0x100065B;
                NRF_RADIO->TIFS     = inter_frame_space_us;
                NRF_RADIO->TXPOWER  = RADIO_TXPOWER_TXPOWER_0dBm << RADIO_TXPOWER_TXPOWER_Pos;

                NRF_RADIO->SHORTS   = 0;
                NRF_RADIO->INTENCLR = 0xffffffff;
            }
        }



        template < typename CallBacks, radio_configuration Configuration >
        radio_base_t< CallBacks, Configuration >* radio_base_t< CallBacks, Configuration >::instance_ = nullptr;

        template < typename CallBacks, radio_configuration Configuration >
        radio_base_t< CallBacks, Configuration >::radio_base_t()
            : state_( state::idle )
            , receive_{ nullptr, 0 }
            , response_{ nullptr, 0 }
            , transmit_time_()
            , accepted_( false )
            , answering_( false )
            , ready_pending_( false )
            , radio_event_pending_( false )
            , radio_event_( event::adv_timeout )
            , radio_event_time_()
            , received_size_( 0 )
            , timer_scheduled_( false )
            , timer_when_()
            , timer_event_pending_( false )
            , rtc_epoch_( 0 )
            , timer_base_( 0 )
            , local_address_()
            , scannable_( false )
            , connectable_( false )
            , scratch_{}
            , reception_{ nullptr, 0 }
            , into_scratch_( false )
            , air_packet_( nullptr )
            , encryption_( nullptr )
            , receive_encrypted_( false )
            , transmit_encrypted_( false )
            , transmitted_encrypted_( false )
            , judgement_pending_( false )
            , disabled_before_answer_( false )
            , connection_()
            , connection_2mbit_( false )
        {
            assert( instance_ == nullptr );
            instance_ = this;

            interrupts = { &radio_base_t::radio_interrupt, &radio_base_t::rtc_interrupt, &radio_base_t::clock_interrupt, &radio_base_t::ccm_interrupt };

            if constexpr ( Configuration.encrypting )
                ccm::enable_interrupt();

            configure_timer( *NRF_TIMER0 );
            configure_rtc();
            configure_radio();
            set_access_address_and_crc_init( advertising_access_address, advertising_crc_init );

            // bias correction on, one value per start; the toolbox starts it per value it draws
            NRF_RNG->CONFIG = RNG_CONFIG_DERCEN_Msk;
            NRF_RNG->SHORTS = RNG_SHORTS_VALRDY_STOP_Msk;

            NVIC_SetPriority( RADIO_IRQn, 0 );
            NVIC_ClearPendingIRQ( RADIO_IRQn );
            NVIC_EnableIRQ( RADIO_IRQn );

            NVIC_SetPriority( RTC0_IRQn, 1 );
            NVIC_ClearPendingIRQ( RTC0_IRQn );
            NVIC_EnableIRQ( RTC0_IRQn );

            NVIC_SetPriority( POWER_CLOCK_IRQn, 1 );
            NVIC_ClearPendingIRQ( POWER_CLOCK_IRQn );
            NVIC_EnableIRQ( POWER_CLOCK_IRQn );

            /*
             * The sleep clock, and the crystal it is synthesized from first if it is; the
             * clock interrupt takes it from there to radio_ready(). The crystal of the other
             * sources is started per event.
             */
            NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
            NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;

            if constexpr ( Configuration.source == sleep_clock::synthesized )
            {
                NRF_CLOCK->LFCLKSRC     = CLOCK_LFCLKSRC_SRC_Synth << CLOCK_LFCLKSRC_SRC_Pos;
                NRF_CLOCK->INTENSET     = CLOCK_INTENSET_HFCLKSTARTED_Msk;
                NRF_CLOCK->TASKS_HFCLKSTART = 1;
            }
            else
            {
                NRF_CLOCK->LFCLKSRC     = ( Configuration.source == sleep_clock::crystal ? CLOCK_LFCLKSRC_SRC_Xtal : CLOCK_LFCLKSRC_SRC_RC ) << CLOCK_LFCLKSRC_SRC_Pos;
                NRF_CLOCK->INTENSET     = CLOCK_INTENSET_LFCLKSTARTED_Msk;
                NRF_CLOCK->TASKS_LFCLKSTART = 1;
            }
        }

        /*
         * The synthesized sleep clock needs the crystal running first; either way the RTC
         * starts with the sleep clock, and the radio is ready then.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::on_clock_event()
        {
            if ( NRF_CLOCK->EVENTS_HFCLKSTARTED && ( NRF_CLOCK->INTENSET & CLOCK_INTENSET_HFCLKSTARTED_Msk ) )
            {
                NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
                NRF_CLOCK->INTENCLR            = CLOCK_INTENCLR_HFCLKSTARTED_Msk;
                NRF_CLOCK->INTENSET            = CLOCK_INTENSET_LFCLKSTARTED_Msk;
                NRF_CLOCK->TASKS_LFCLKSTART    = 1;
            }

            if ( NRF_CLOCK->EVENTS_LFCLKSTARTED && ( NRF_CLOCK->INTENSET & CLOCK_INTENSET_LFCLKSTARTED_Msk ) )
            {
                NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
                NRF_CLOCK->INTENCLR            = CLOCK_INTENCLR_LFCLKSTARTED_Msk;

                NRF_RTC0->TASKS_START = 1;

                ready_pending_ = true;
                __SEV();
            }
        }

        /*
         * The overflow keeps the ticks counting past the RTC's 24 bits; the user timer's
         * compare expires it.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::on_rtc_event()
        {
            if ( NRF_RTC0->EVENTS_OVRFLW )
            {
                NRF_RTC0->EVENTS_OVRFLW = 0;
                rtc_epoch_ = rtc_epoch_ + 1;
            }

            if ( NRF_RTC0->EVENTS_COMPARE[ rtc_cc_user_timer ] && ( NRF_RTC0->INTENSET & RTC_INTENSET_COMPARE2_Msk ) )
                on_timer_expired();
        }

        /*
         * The counter with the overflows in front of it; an overflow the interrupt has not
         * counted yet, because interrupts are off, shows as its event with a counter that
         * wrapped.
         */
        template < typename CallBacks, radio_configuration Configuration >
        std::uint32_t radio_base_t< CallBacks, Configuration >::ticks_now() const
        {
            std::uint32_t epoch   = rtc_epoch_;
            std::uint32_t counter = NRF_RTC0->COUNTER;

            if ( NRF_RTC0->EVENTS_OVRFLW && counter < ( 1u << ( rtc_bits - 1 ) ) )
                ++epoch;

            return ( epoch << rtc_bits ) | counter;
        }

        // a multiplication and a shift, the denominator being a power of two
        template < typename CallBacks, radio_configuration Configuration >
        std::uint32_t radio_base_t< CallBacks, Configuration >::microseconds_of( std::uint64_t ticks )
        {
            return static_cast< std::uint32_t >( ( ticks * us_per_tick_numerator ) >> 9 );
        }

        /*
         * TIMER0, cleared, starts at the last tick before `from`, and the crystal a startup
         * time earlier; timer_base_ is that tick in microseconds, which is what TIMER0 then
         * counts from. The caller keeps interrupts off from the reading of the clock the
         * request was checked against, so the compares are in the future when written.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::place_timer( link_layer::abs_time from )
        {
            // the tick before `from` is this tick plus the whole ticks ahead, as this tick is whole
            const std::uint32_t ticks = ticks_now();
            const std::int32_t  ahead = static_cast< std::int32_t >( from.data() - microseconds_of( ticks ) );
            assert( ahead > 0 );

            const std::uint32_t target_tick = ticks + ticks_of( static_cast< std::uint32_t >( ahead ), false );
            assert( target_tick - ticks >= rtc_compare_lead_ticks + hfxo_startup_ticks< Configuration > );

            timer_base_ = microseconds_of( target_tick );

            NRF_TIMER0->TASKS_STOP  = 1;
            NRF_TIMER0->TASKS_CLEAR = 1;

            NRF_RTC0->EVENTS_COMPARE[ rtc_cc_hfxo ]  = 0;
            NRF_RTC0->EVENTS_COMPARE[ rtc_cc_timer ] = 0;
            NRF_RTC0->CC[ rtc_cc_hfxo ]  = ( target_tick - hfxo_startup_ticks< Configuration > ) & ( ( 1u << rtc_bits ) - 1 );
            NRF_RTC0->CC[ rtc_cc_timer ] = target_tick & ( ( 1u << rtc_bits ) - 1 );

            NRF_PPI->CHENSET = ppi_rtc_hfxo | ppi_rtc_timer;
        }

        /*
         * The event is over: TIMER0 stops, and so does the crystal unless the sleep clock
         * comes from it. The times of the event were taken before.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::release_clocks()
        {
            NRF_PPI->CHENCLR        = ppi_rtc_hfxo | ppi_rtc_timer;
            NRF_TIMER0->TASKS_STOP  = 1;
            NRF_TIMER0->TASKS_CLEAR = 1;

            NRF_RTC0->EVENTS_COMPARE[ rtc_cc_hfxo ]  = 0;
            NRF_RTC0->EVENTS_COMPARE[ rtc_cc_timer ] = 0;

            if constexpr ( Configuration.source != sleep_clock::synthesized )
                NRF_CLOCK->TASKS_HFCLKSTOP = 1;
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::sleep()
        {
            __WFE();
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::wake_up()
        {
            __SEV();
        }

        /*
         * To the microsecond from TIMER0 while it runs, which the RTC's compare that started
         * it tells; to the tick from the RTC otherwise.
         */
        template < typename CallBacks, radio_configuration Configuration >
        link_layer::abs_time radio_base_t< CallBacks, Configuration >::now() const
        {
            if ( NRF_RTC0->EVENTS_COMPARE[ rtc_cc_timer ] )
            {
                NRF_TIMER0->TASKS_CAPTURE[ cc_now ] = 1;

                return link_layer::abs_time( timer_base_ + NRF_TIMER0->CC[ cc_now ] );
            }

            return link_layer::abs_time( microseconds_of( ticks_now() ) );
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
        {
            // changed only while no action is pending
            assert( state_ == state::idle );

            NRF_RADIO->BASE0    = access_address << 8;
            NRF_RADIO->PREFIX0  = access_address >> 24;
            NRF_RADIO->CRCINIT  = crc_init;
        }

        /*
         * Taken over by the next connection event scheduled. Only a symmetric PHY is
         * implemented, both directions on the same one; an unchanged direction keeps its PHY.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::set_phy( link_layer::phy_ll_encoding::phy_ll_encoding_t receiving, link_layer::phy_ll_encoding::phy_ll_encoding_t transmitting )
        {
            using namespace link_layer::phy_ll_encoding;

            const bool receive_2mbit  = receiving == le_unchanged_coding ? connection_2mbit_ : receiving == le_2m_phy;
            const bool transmit_2mbit = transmitting == le_unchanged_coding ? connection_2mbit_ : transmitting == le_2m_phy;

            assert( receive_2mbit == transmit_2mbit );
            static_cast< void >( transmit_2mbit );

            connection_2mbit_ = receive_2mbit;
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::set_local_address( const link_layer::device_address& address )
        {
            local_address_ = address;
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::set_encryption( encryption_t& encryption )
        {
            encryption_            = &encryption;
            transmitted_encrypted_ = false;
        }


        /*
         * Whether what was received is a scan request this device has to answer: the
         * advertisement it follows could be scanned, the PDU is a scan request of the
         * size one has, and the address it is addressed to, the second address field,
         * is this device's. Whether that scanner may be answered is then the acceptance
         * filter's decision, not this one's.
         */
        /*
         * A scan or connect request is addressed to this device when its AdvA, the second
         * address it carries, is the local address of the same kind.
         */
        static bool addressed_to( const link_layer::read_buffer& received, const link_layer::device_address& local_address )
        {
            if ( received.size < addressed_to_offset + address_size )
                return false;

            const bool is_random = received.buffer[ 0 ] & rx_add_mask;

            return is_random == local_address.is_random()
                && std::equal( local_address.begin(), local_address.end(), &received.buffer[ addressed_to_offset ] );
        }

        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::is_scan_request_for_us() const
        {
            return scannable_
                && ( receive_.buffer[ 0 ] & pdu_type_mask ) == scan_request_type
                && receive_.buffer[ 1 ] == scan_request_payload_size
                && addressed_to( receive_, local_address_ );
        }

        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::is_connect_request_for_us() const
        {
            return connectable_
                && ( receive_.buffer[ 0 ] & pdu_type_mask ) == connect_request_type
                && receive_.buffer[ 1 ] == connect_request_payload_size
                && addressed_to( receive_, local_address_ );
        }

        /*
         * The acceptance filter of scheduled_radio2.hpp: the sender of an advertising
         * channel PDU is its first address field, the six bytes after the two byte header,
         * public or random by the header's TxAdd bit.
         */
        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::sender_in_acceptance_filter()
        {
            const bool is_random = receive_.buffer[ 0 ] & tx_add_mask;
            const link_layer::device_address sender( &receive_.buffer[ memory_header_size ], is_random );

            return callbacks().is_in_acceptance_filter( sender );
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::start_advertising_event(
            std::uint32_t                       channel,
            const link_layer::write_buffer&     transmit,
            const link_layer::write_buffer&     response,
            const link_layer::read_buffer&      receive )
        {
            const interrupts_off no_interruption;

            schedule( channel, now() + link_layer::delta_time::usec( earliest_us< Configuration > ), transmit, response, receive );
        }

        template < typename CallBacks, radio_configuration Configuration >
        std::uint32_t radio_base_t< CallBacks, Configuration >::static_random_address_seed() const
        {
            return NRF_FICR->DEVICEID[ 0 ];
        }

        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::schedule_advertising_event(
            std::uint32_t                       channel,
            link_layer::abs_time                when,
            const link_layer::write_buffer&     transmit,
            const link_layer::write_buffer&     response,
            const link_layer::read_buffer&      receive )
        {
            const interrupts_off no_interruption;

            if ( when.is_in_near_past( now() + link_layer::delta_time::usec( earliest_us< Configuration > ) ) )
                return false;

            schedule( channel, when, transmit, response, receive );

            return true;
        }

        /*
         * The transmission starts when TIMER0 reaches compare 0, through PPI into TXEN, a
         * ramp up before the first bit is due; the end of the packet lands in capture 2
         * through PPI, and the shorts take the radio from there into receiving. The
         * interrupt on DISABLED then finishes the setup of the receiver.
         *
         * Called with interrupts disabled, from the moment its caller read the clock the
         * start is placed against. A PPI channel forwards an event as it happens and does
         * nothing for one that already did, so a compare that passes before the channel is
         * enabled is simply lost: the transmission never starts, no callback is ever
         * delivered, and the radio is left pending on an action that cannot end. The margin
         * in earliest_us is what the setup needs, and holding interrupts off for its few
         * microseconds is what makes that margin a guarantee rather than a likelihood.
         *
         * It also keeps the radio's own interrupt out of the check of state_.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::schedule(
            std::uint32_t                       channel,
            link_layer::abs_time                when,
            const link_layer::write_buffer&     transmit,
            const link_layer::write_buffer&     response,
            const link_layer::read_buffer&      receive )
        {
            assert( receive.buffer && receive.size >= 2 );
            assert( transmit.buffer && transmit.size >= 2 );

            // pending lasts until the callback is delivered; see next_event()
            assert( state_ == state::idle );

            receive_        = receive;
            response_       = response;
            transmit_time_  = when;
            scannable_      = is_scannable( transmit.buffer[ 0 ] );
            connectable_    = is_connectable( transmit.buffer[ 0 ] );
            accepted_       = false;
            answering_      = false;

            // advertising is on the 1 Mbit PHY, whatever the connections use
            configure_phy( false );

            NRF_RADIO->FREQUENCY    = frequency_from_channel( channel );
            NRF_RADIO->DATAWHITEIV  = channel & 0x3f;
            NRF_RADIO->PACKETPTR    = reinterpret_cast< std::uint32_t >( transmit.buffer );
            NRF_RADIO->PCNF1        = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( ( transmit.size - memory_header_size ) << RADIO_PCNF1_MAXLEN_Pos );
            NRF_RADIO->SHORTS       =
                  RADIO_SHORTS_READY_START_Msk
                | RADIO_SHORTS_END_DISABLE_Msk
                | RADIO_SHORTS_DISABLED_RXEN_Msk;

            NRF_RADIO->EVENTS_READY     = 0;
            NRF_RADIO->EVENTS_ADDRESS   = 0;
            NRF_RADIO->EVENTS_END       = 0;
            NRF_RADIO->EVENTS_DISABLED  = 0;

            place_timer( when - link_layer::delta_time::usec( ramp_up_us + 1 ) );

            NRF_TIMER0->EVENTS_COMPARE[ cc_start ]      = 0;
            NRF_TIMER0->EVENTS_COMPARE[ cc_window_end ] = 0;
            NRF_TIMER0->CC[ cc_start ]                  = when.data() - ramp_up_us - timer_base_;

            NRF_PPI->CHENCLR = ppi_compare0_rxen | ppi_compare1_disable;
            NRF_PPI->CHENSET = ppi_compare0_txen | ppi_end_capture2;

            state_ = state::transmitting;

            NRF_RADIO->INTENSET = RADIO_INTENSET_DISABLED_Msk;
        }

        /*
         * The receiver starts when TIMER0 reaches compare 0, a ramp up before `start`, and
         * compare 1 disables it if no packet began by `end`: a packet that began by then has its
         * address received before the compare, and the address interrupt stops it. From there the
         * event runs in the interrupts, like an advertising event; see on_connection_packet_end().
         */
        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::schedule_connection_event( std::uint32_t channel, link_layer::abs_time start, link_layer::abs_time end )
        {
            const interrupts_off no_interruption;

            // end lies after start; the comparison is on the ring of abs_time
            assert( !end.is_in_near_past( start + link_layer::delta_time::usec( 1 ) ) );

            if ( start.is_in_near_past( now() + link_layer::delta_time::usec( earliest_us< Configuration > ) ) )
                return false;

            // pending lasts until the callback is delivered; see next_event()
            assert( state_ == state::idle );

            connection_     = connection_event_state();
            connection_.end = end;

            configure_phy( connection_2mbit_ );

            NRF_RADIO->FREQUENCY    = frequency_from_channel( channel );
            NRF_RADIO->DATAWHITEIV  = channel & 0x3f;
            NRF_RADIO->SHORTS       = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

            if constexpr ( Configuration.encrypting )
            {
                receive_encrypted_  = encryption_ && encryption_->receive_encrypted;
                transmit_encrypted_ = encryption_ && encryption_->transmit_encrypted;

                if ( receive_encrypted_ || transmit_encrypted_ )
                    ccm::begin_event( *encryption_ );
            }

            allocate_connection_reception();

            NRF_RADIO->EVENTS_READY     = 0;
            NRF_RADIO->EVENTS_ADDRESS   = 0;
            NRF_RADIO->EVENTS_END       = 0;
            NRF_RADIO->EVENTS_DISABLED  = 0;

            place_timer( start - link_layer::delta_time::usec( ramp_up_us + 1 ) );

            NRF_TIMER0->EVENTS_COMPARE[ cc_start ]      = 0;
            NRF_TIMER0->EVENTS_COMPARE[ cc_window_end ] = 0;
            NRF_TIMER0->CC[ cc_start ]                  = start.data() - ramp_up_us - timer_base_;
            NRF_TIMER0->CC[ cc_window_end ]             = end.data() + address_of_a_packet_at_end_us( timing( connection_2mbit_ ) ) - timer_base_;

            NRF_PPI->CHENCLR = ppi_compare0_txen;
            NRF_PPI->CHENSET = ppi_compare0_rxen | ppi_compare1_disable | ppi_end_capture2;

            state_ = state::connection_receiving;

            NRF_RADIO->INTENSET = RADIO_INTENSET_ADDRESS_Msk | RADIO_INTENSET_END_Msk | RADIO_INTENSET_DISABLED_Msk;

            return true;
        }

        /*
         * Definitive against the start of the event. Before TIMER0 started, the RTC's
         * compare is the start: with its channel off and the compare's tick still ahead,
         * nothing starts. From the tick on, the channel that starts the transmission or the
         * receiver is the start: once it is off, the radio is either still disabled, in which
         * case the start can no longer happen, or it has begun to ramp up, in which case the
         * event proceeds; a few microseconds cover the cycle the task takes.
         */
        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::cancel_radio_event()
        {
            const interrupts_off no_interruption;

            const bool before_start = state_ == state::transmitting
                || ( state_ == state::connection_receiving && !connection_.received_any );

            if ( !before_start )
                return false;

            NRF_PPI->CHENCLR = ppi_rtc_timer;

            const std::uint32_t ticks_ahead = ( NRF_RTC0->CC[ rtc_cc_timer ] - NRF_RTC0->COUNTER ) & ( ( 1u << rtc_bits ) - 1 );

            if ( ticks_ahead > 1 && ticks_ahead < ( 1u << ( rtc_bits - 1 ) ) )
            {
                NRF_RADIO->INTENCLR = RADIO_INTENCLR_ADDRESS_Msk | RADIO_INTENCLR_END_Msk | RADIO_INTENCLR_DISABLED_Msk;
                NRF_RADIO->SHORTS   = 0;
                NRF_PPI->CHENCLR    = ppi_compare0_txen | ppi_compare0_rxen | ppi_compare1_disable | ppi_end_capture2;
                release_clocks();

                state_ = state::idle;

                return true;
            }

            // the tick is here or gone: TIMER0 starts, or started; the radio's start is the question
            NRF_PPI->CHENSET = ppi_rtc_timer;
            NRF_PPI->CHENCLR = ppi_compare0_txen | ppi_compare0_rxen;

            for ( int cycles = 0; cycles != 256; ++cycles )
                __NOP();

            if ( !radio_disabled() )
                return false;

            NRF_RADIO->INTENCLR = RADIO_INTENCLR_ADDRESS_Msk | RADIO_INTENCLR_END_Msk | RADIO_INTENCLR_DISABLED_Msk;
            NRF_RADIO->SHORTS   = 0;
            NRF_PPI->CHENCLR    = ppi_compare1_disable | ppi_end_capture2;
            release_clocks();

            state_ = state::idle;

            return true;
        }

        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::schedule_timer( link_layer::abs_time when )
        {
            const interrupts_off no_interruption;

            assert( !timer_scheduled_ );

            // the tick after `when`, and the RTC needs its compare ahead
            const std::uint32_t ticks = ticks_now();
            const std::int32_t  ahead = static_cast< std::int32_t >( when.data() - microseconds_of( ticks ) );

            if ( ahead <= static_cast< std::int32_t >( ( rtc_compare_lead_ticks + 1 ) * tick_us ) )
                return false;

            const std::uint32_t tick = ticks + ticks_of( static_cast< std::uint32_t >( ahead ), true );

            timer_when_      = when;
            timer_scheduled_ = true;

            NRF_RTC0->EVENTS_COMPARE[ rtc_cc_user_timer ] = 0;
            NRF_RTC0->CC[ rtc_cc_user_timer ]             = tick & ( ( 1u << rtc_bits ) - 1 );
            NRF_RTC0->INTENSET = RTC_INTENSET_COMPARE2_Msk;

            return true;
        }

        /*
         * The compare event is set by the hardware whether or not it interrupts, so once
         * the interrupt is off, the event tells whether the timer expired in the meantime;
         * if it did, the interrupt is turned on again and delivers it.
         */
        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::cancel_timer()
        {
            const interrupts_off no_interruption;

            if ( !timer_scheduled_ )
                return false;

            NRF_RTC0->INTENCLR = RTC_INTENCLR_COMPARE2_Msk;

            if ( NRF_RTC0->EVENTS_COMPARE[ rtc_cc_user_timer ] || timer_event_pending_ )
            {
                NRF_RTC0->INTENSET = RTC_INTENSET_COMPARE2_Msk;

                return false;
            }

            timer_scheduled_ = false;

            return true;
        }

        template < typename CallBacks, radio_configuration Configuration >
        std::optional< typename radio_base_t< CallBacks, Configuration >::happened > radio_base_t< CallBacks, Configuration >::next_event()
        {
            const interrupts_off no_interruption;

            if ( ready_pending_ )
            {
                ready_pending_ = false;

                return happened{ event::radio_ready, link_layer::abs_time(), { nullptr, 0 }, {} };
            }

            if ( radio_event_pending_ )
            {
                // handed out now, so the action stops being pending now
                radio_event_pending_ = false;
                state_               = state::idle;

                return happened{ radio_event_, radio_event_time_, { receive_.buffer, received_size_ }, connection_.events };
            }

            if ( timer_event_pending_ )
            {
                timer_event_pending_ = false;
                timer_scheduled_     = false;

                return happened{ event::user_timer, timer_when_, { nullptr, 0 }, {} };
            }

            return std::nullopt;
        }

        /*
         * The end of a packet of an advertising event: the request that was received, or
         * the answer going out. The reception is judged here rather than at the disable
         * that follows it, because the answer has to be armed while the radio is still
         * disabling for the radio's own inter frame spacing to place it.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::on_packet_end()
        {
            NRF_RADIO->EVENTS_END = 0;

            if ( state_ == state::connection_receiving || state_ == state::connection_transmitting || state_ == state::connection_closing )
            {
                on_connection_packet_end();
                return;
            }

            if ( state_ == state::responding )
            {
                // the answer is out; the chain stops here, so the disable it causes ends
                // the event rather than starting a second transmission
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
                answering_        = false;

                return;
            }

            if ( state_ != state::receiving )
                return;

            const bool crc_ok          = received_crc_ok();
            const bool scan_request    = crc_ok && is_scan_request_for_us();
            const bool connect_request = crc_ok && is_connect_request_for_us();

            if ( !( scan_request || connect_request ) || !sender_in_acceptance_filter() )
            {
                cancel_answer();
                return;
            }

            // the time and the bytes are taken now, before the answer repoints the radio
            // and its own end captures the timer again
            const std::uint32_t payload_size = receive_.buffer[ 1 ];

            received_size_    = std::min< std::size_t >( payload_size + memory_header_size, receive_.size );
            radio_event_time_ = link_layer::abs_time( timer_base_ + NRF_TIMER0->CC[ cc_packet_end ] - air_time_us( le_1m_timing, payload_size ) );
            accepted_         = true;

            // a connect request is reported and not answered; the event ends with it
            if ( connect_request )
            {
                cancel_answer();
                return;
            }

            if ( !answer_armed() )
                return;

            // the transmitter is on its way already; it only needs to know what to send
            NRF_RADIO->PACKETPTR = reinterpret_cast< std::uint32_t >( response_.buffer );
            NRF_RADIO->PCNF1     = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( ( response_.size - memory_header_size ) << RADIO_PCNF1_MAXLEN_Pos );

            answering_ = true;
            state_     = state::responding;
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::on_radio_disabled()
        {
            NRF_RADIO->EVENTS_DISABLED = 0;

            if ( state_ == state::connection_receiving || state_ == state::connection_transmitting || state_ == state::connection_closing )
            {
                on_connection_disabled();
                return;
            }

            if ( state_ == state::transmitting )
            {
                // the short is ramping the receiver up already; it gets the buffer and its deadline
                NRF_RADIO->EVENTS_END   = 0;
                NRF_RADIO->PACKETPTR    = reinterpret_cast< std::uint32_t >( receive_.buffer );
                NRF_RADIO->PCNF1        = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( ( receive_.size - memory_header_size ) << RADIO_PCNF1_MAXLEN_Pos );
                NRF_RADIO->SHORTS       = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

                // the reception is judged at its end; an answer is armed at its address
                NRF_RADIO->EVENTS_ADDRESS = 0;
                NRF_RADIO->INTENSET     = RADIO_INTENSET_END_Msk | ( can_answer() ? RADIO_INTENSET_ADDRESS_Msk : 0 );

                NRF_TIMER0->EVENTS_COMPARE[ cc_window_end ] = 0;
                NRF_TIMER0->CC[ cc_window_end ]             = NRF_TIMER0->CC[ cc_packet_end ] + response_window_us;
                NRF_PPI->CHENSET = ppi_compare1_disable;

                state_ = state::receiving;
            }
            else if ( answering_ )
            {
                // the short has started the transmitter; the end of the answer must not start another
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
            }
            else if ( ( state_ == state::receiving || state_ == state::responding ) && radio_disabled() )
            {
                // a cancelled transmitter disables a second time, so only a disabled radio ends the event
                end_event();
            }
        }

        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::can_answer() const
        {
            return scannable_ && response_.buffer != nullptr && response_.size >= 2;
        }

        template < typename CallBacks, radio_configuration Configuration >
        bool radio_base_t< CallBacks, Configuration >::answer_armed() const
        {
            return NRF_RADIO->SHORTS & RADIO_SHORTS_DISABLED_TXEN_Msk;
        }

        /*
         * The address of a packet in the receive window of a scannable advertisement. TIFS
         * holds only when the shorts that turn the end of the packet into the transmitter's
         * ramp up are in place before the packet ends, so the answer is armed here and
         * cancelled at the end if the packet is not one to answer. A window that closed
         * before this interrupt disabled the receiver already, and nothing is armed.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::on_address()
        {
            NRF_RADIO->EVENTS_ADDRESS = 0;

            // in a connection event every packet received is answered, and the interrupt stays on
            if ( state_ == state::connection_receiving )
            {
                NRF_PPI->CHENCLR = ppi_compare1_disable;

                if ( !NRF_TIMER0->EVENTS_COMPARE[ cc_window_end ] )
                    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;

                return;
            }

            // the address of the device's own answer; the next reception needs the interrupt
            if ( state_ == state::connection_transmitting || state_ == state::connection_closing )
                return;

            NRF_RADIO->INTENCLR       = RADIO_INTENCLR_ADDRESS_Msk;

            if ( state_ != state::receiving )
                return;

            // the window must not cut the packet, nor a possible answer, off
            NRF_PPI->CHENCLR = ppi_compare1_disable;

            if ( NRF_TIMER0->EVENTS_COMPARE[ cc_window_end ] )
                return;

            NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk | RADIO_SHORTS_DISABLED_TXEN_Msk;
        }

        /*
         * The short is removed in case the receiver is still disabling, and a transmitter
         * it started already is ramping up and is disabled again.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::cancel_answer()
        {
            if ( !answer_armed() )
                return;

            NRF_RADIO->SHORTS        = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
            NRF_RADIO->TASKS_DISABLE = 1;
        }

        /*
         * Ends the advertising event and reports it: what was accepted at the end of the
         * received packet, or a timeout carrying the time this event's own transmission
         * began, so that a caller can chain intervals from it.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::end_event()
        {
            NRF_PPI->CHENCLR    = ppi_compare0_txen | ppi_compare1_disable | ppi_end_capture2;
            NRF_RADIO->SHORTS   = 0;
            NRF_RADIO->INTENCLR = RADIO_INTENCLR_DISABLED_Msk | RADIO_INTENCLR_END_Msk | RADIO_INTENCLR_ADDRESS_Msk;

            if ( accepted_ )
            {
                radio_event_ = event::adv_received;
            }
            else
            {
                radio_event_time_ = transmit_time_;
                radio_event_      = event::adv_timeout;
            }

            release_clocks();

            state_               = state::reporting;
            answering_           = false;
            radio_event_pending_ = true;
            __SEV();
        }

        /*
         * The room of the buffer for the next PDU, or the scratch if the buffer has none; a PDU
         * received into the scratch is not stored, and its sender gets no acknowledgement.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::allocate_connection_reception()
        {
            reception_    = buffer().allocate_receive_buffer();
            into_scratch_ = reception_.size == 0;

            if ( into_scratch_ )
                reception_ = link_layer::read_buffer{ scratch_, sizeof( scratch_ ) };

            // encrypted, the RADIO receives the ciphertext into the scratch and the CCM fills the room;
            // without room, the ciphertext stays what it is, and nothing is taken from it
            link_layer::read_buffer target = reception_;

            if constexpr ( Configuration.encrypting )
            {
                if ( receive_encrypted_ && !into_scratch_ )
                    target = ccm::prepare_reception( *encryption_, reception_, { scratch_, sizeof( scratch_ ) }, connection_2mbit_ );
            }

            air_packet_             = target.buffer;
            judgement_pending_      = false;
            disabled_before_answer_ = false;

            NRF_RADIO->PACKETPTR = reinterpret_cast< std::uint32_t >( target.buffer );
            NRF_RADIO->PCNF1     = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( ( target.size - memory_header_size ) << RADIO_PCNF1_MAXLEN_Pos );
        }

        /*
         * The end of a packet of a connection event.
         *
         * A received packet is answered from the buffer: a PDU with a valid CRC goes into it,
         * and the answer acknowledges it; one the buffer had no room for, and one with an invalid
         * CRC, get the answer without an acknowledgement, and the second invalid CRC in a row
         * cancels the transmitter and closes the event.
         * The transmitter is already on its way, armed at the address, and TIFS places it.
         *
         * The end of the answer decides what follows it: with more data on either side the
         * receiver, which the DISABLED to RXEN short ramps up, for the answer window; otherwise
         * nothing, and the disable closes the event.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::on_connection_packet_end()
        {
            if ( state_ == state::connection_transmitting )
            {
                if ( !connection_.continues )
                {
                    state_ = state::connection_closing;
                    return;
                }

                allocate_connection_reception();

                NRF_TIMER0->EVENTS_COMPARE[ cc_window_end ] = 0;
                NRF_TIMER0->CC[ cc_window_end ]             = NRF_TIMER0->CC[ cc_packet_end ] + connection_answer_window_us( timing( connection_2mbit_ ) );
                NRF_PPI->CHENSET = ppi_compare1_disable;

                state_ = state::connection_receiving;

                return;
            }

            if ( state_ != state::connection_receiving )
                return;

            const bool          crc_ok       = received_crc_ok();
            const std::uint8_t  header       = air_packet_[ 0 ];
            const std::uint32_t payload_size = air_packet_[ 1 ];

            // the CCM is not to be started by the address of the answer
            NRF_PPI->CHENCLR = ppi_address_ccm_crypt;

            // the anchor is the first bit of the first packet, whatever its CRC
            if ( !connection_.received_any )
            {
                connection_.anchor       = link_layer::abs_time( timer_base_ + NRF_TIMER0->CC[ cc_packet_end ] - air_time_us( timing( connection_2mbit_ ), payload_size ) );
                connection_.received_any = true;
            }

            // the window closed before the address, and the disable ends the event
            if ( !answer_armed() && !judgement_pending_ )
                return;

            if constexpr ( Configuration.encrypting )
            {
                // the CCM finishes after the packet; its interrupt gets here again
                if ( crc_ok && receive_encrypted_ && !into_scratch_ && ccm::decryption_pending( payload_size ) )
                {
                    judgement_pending_ = true;
                    return;
                }

                judgement_pending_ = false;
            }

            link_layer::write_buffer answer{ nullptr, 0 };

            if ( crc_ok )
            {
                connection_.crc_errors_in_a_row = 0;

                connection_.events.last_received_not_empty     = payload_size != 0;
                connection_.events.last_received_had_more_data = header & md_mask;

                // without room nothing is taken from the PDU, not even its acknowledgement, so
                // the PDU sent before goes out again unchanged
                if ( into_scratch_ )
                {
                    answer = buffer().next_transmit();
                }
                else
                {
                    if ( connection_.unacknowledged && static_cast< bool >( header & nesn_mask ) != connection_.unacknowledged_sn )
                        connection_.unacknowledged = false;

                    /*
                     * A PDU whose MIC does not check is not taken, but what it acknowledges is.
                     * The counters advance with the encrypted PDUs only: a new one taken from
                     * the air, and a transmitted one the peer acknowledges; empty PDUs and
                     * the PDUs from before a switch went in plain.
                     */
                    if constexpr ( Configuration.encrypting )
                    {
                        const bool authentic = !receive_encrypted_
                            || ccm::reception_authentic( reception_, air_packet_, payload_size );

                        const link_layer::reception_result result = authentic
                            ? buffer().received( reception_ )
                            : buffer().acknowledge( reception_ );

                        ccm::advance( *encryption_,
                            receive_encrypted_ && payload_size != 0 && result.received_new_pdu,
                            transmitted_encrypted_ && result.acknowledged_pdu );

                        answer = result.transmit;
                    }
                    else
                    {
                        answer = buffer().received( reception_ ).transmit;
                    }
                }
            }
            else
            {
                connection_.events.error_occured = true;

                if ( ++connection_.crc_errors_in_a_row == 2 )
                {
                    NRF_RADIO->SHORTS        = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
                    NRF_RADIO->TASKS_DISABLE = 1;
                    state_                   = state::connection_closing;

                    return;
                }

                answer = buffer().next_transmit();
            }

            // what goes on air: the answer, or its ciphertext from the scratch
            link_layer::write_buffer sent = answer;

            if constexpr ( Configuration.encrypting )
            {
                if ( transmit_encrypted_ )
                    sent = ccm::prepare_transmission( *encryption_, answer, { scratch_, sizeof( scratch_ ) }, connection_2mbit_ );

                transmitted_encrypted_ = sent.buffer != answer.buffer;
            }

            NRF_RADIO->PACKETPTR = reinterpret_cast< std::uint32_t >( sent.buffer );
            NRF_RADIO->PCNF1     = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( ( sent.size - memory_header_size ) << RADIO_PCNF1_MAXLEN_Pos );

            const std::uint8_t answer_header = answer.buffer[ 0 ];

            connection_.last_transmitted_more_data        = answer_header & md_mask;
            connection_.events.last_transmitted_not_empty = answer.buffer[ 1 ] != 0;

            if ( answer.buffer[ 1 ] != 0 )
            {
                connection_.unacknowledged    = true;
                connection_.unacknowledged_sn = answer_header & sn_mask;
            }

            // the MD bit of a PDU with an invalid CRC is unknown; the central decides with its next
            connection_.continues = !crc_ok || ( header & md_mask ) || connection_.last_transmitted_more_data;
            state_     = state::connection_transmitting;

            if constexpr ( Configuration.encrypting )
            {
                // judged after the disable that started the answer: what that disable would have set
                if ( disabled_before_answer_ )
                {
                    disabled_before_answer_ = false;
                    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk
                        | ( connection_.continues ? RADIO_SHORTS_DISABLED_RXEN_Msk : 0 );
                }
            }
        }

        /*
         * The disables of a connection event. The one between a reception and its answer is
         * where the short started the transmitter, so it is taken out again, and the one
         * after the answer ramps the receiver up if the event goes on. A disable that leaves
         * the radio disabled ends the event: the window closed, the last answer is out, or the
         * transmitter was cancelled.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::on_connection_disabled()
        {
            if constexpr ( Configuration.encrypting )
            {
                // the transmitter is on its way while the reception is still being judged
                if ( judgement_pending_ )
                {
                    disabled_before_answer_ = true;
                    return;
                }
            }

            if ( state_ == state::connection_transmitting )
            {
                NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk
                    | ( connection_.continues ? RADIO_SHORTS_DISABLED_RXEN_Msk : 0 );

                return;
            }

            if ( !radio_disabled() )
            {
                // the receiver ramps up after the answer; the next disable must not start it again
                NRF_RADIO->SHORTS = NRF_RADIO->SHORTS & ~RADIO_SHORTS_DISABLED_RXEN_Msk;

                return;
            }

            end_connection_event();
        }

        /*
         * Reports the connection event: its end with the anchor and what happened, or a
         * timeout carrying `end` if nothing was received.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::end_connection_event()
        {
            NRF_PPI->CHENCLR    = ppi_compare0_rxen | ppi_compare0_txen | ppi_compare1_disable | ppi_end_capture2 | ppi_address_ccm_crypt;
            NRF_RADIO->SHORTS   = 0;
            NRF_RADIO->INTENCLR = RADIO_INTENCLR_ADDRESS_Msk | RADIO_INTENCLR_END_Msk | RADIO_INTENCLR_DISABLED_Msk;

            if ( connection_.received_any )
            {
                // more to send: the last answer said so, or data came after an empty one
                connection_.events.unacknowledged_data   = connection_.unacknowledged;
                connection_.events.pending_outgoing_data = connection_.last_transmitted_more_data
                    || ( !connection_.events.last_transmitted_not_empty && buffer().pending_outgoing_data_available() );

                radio_event_      = event::connection_end_event;
                radio_event_time_ = connection_.anchor;
            }
            else
            {
                radio_event_      = event::connection_timeout;
                radio_event_time_ = connection_.end;
            }

            release_clocks();

            received_size_       = 0;
            state_               = state::reporting;
            radio_event_pending_ = true;
            __SEV();
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::on_timer_expired()
        {
            NRF_RTC0->EVENTS_COMPARE[ rtc_cc_user_timer ] = 0;
            NRF_RTC0->INTENCLR = RTC_INTENCLR_COMPARE2_Msk;

            timer_event_pending_ = true;
            __SEV();
        }

        /*
         * END before DISABLED: the short between them can leave both pending together,
         * and what the end of the packet decided is what the disable then acts on.
         */
        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::radio_interrupt()
        {
            if ( !instance_ )
                return;

            if ( NRF_RADIO->EVENTS_ADDRESS && ( NRF_RADIO->INTENSET & RADIO_INTENSET_ADDRESS_Msk ) )
                instance_->on_address();

            if ( NRF_RADIO->EVENTS_END )
                instance_->on_packet_end();

            if ( NRF_RADIO->EVENTS_DISABLED )
                instance_->on_radio_disabled();
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::ccm_interrupt()
        {
            if ( !instance_ || !instance_->judgement_pending_ )
                return;

            instance_->on_connection_packet_end();
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::rtc_interrupt()
        {
            if ( instance_ )
                instance_->on_rtc_event();
        }

        template < typename CallBacks, radio_configuration Configuration >
        void radio_base_t< CallBacks, Configuration >::clock_interrupt()
        {
            if ( instance_ )
                instance_->on_clock_event();
        }
    }
}


#endif
