#include <bluetoe/nrf52_radio.hpp>

#include <nrf.h>

#include <algorithm>
#include <cassert>

namespace bluetoe
{
    namespace nrf52_details
    {
        namespace {

            /*
             * Both timers count microseconds: the 16 MHz peripheral clock through a
             * prescaler of 2^4.
             */
            constexpr std::uint32_t prescaler_for_1us = 4;

            /*
             * TIMER0 belongs to the radio: compare 0 starts a transmission, compare 1 ends
             * the receive window, capture 2 takes the end of a packet, capture 3 reads the
             * time. TIMER1, its twin, holds the user timer in compare 0.
             */
            constexpr std::size_t cc_start          = 0;
            constexpr std::size_t cc_window_end     = 1;
            constexpr std::size_t cc_packet_end     = 2;
            constexpr std::size_t cc_now            = 3;
            constexpr std::size_t cc_user_timer     = 0;

            /*
             * The pre-programmed PPI channels of the nRF52, and one free channel that starts
             * both timers in the same cycle from an event generator task.
             */
            constexpr std::uint32_t ppi_compare0_txen       = 1u << 20;
            constexpr std::uint32_t ppi_compare0_rxen       = 1u << 21;
            constexpr std::uint32_t ppi_compare1_disable    = 1u << 22;
            constexpr std::uint32_t ppi_end_capture2        = 1u << 27;
            constexpr std::size_t   ppi_start_timers        = 0;

            /*
             * With MODECNF0.RU set, the radio ramps up in 40 µs on every nRF52.
             */
            constexpr std::uint32_t ramp_up_us = 40;

            /*
             * How far ahead a request has to be for the radio to be set up in time: the
             * ramp up, and a margin for the setup itself and an interrupt in between.
             */
            constexpr std::uint32_t earliest_us = ramp_up_us + 60;

            /*
             * The receive window after an advertising PDU: the inter frame space, the
             * longest legacy response (CONNECT_IND: preamble, access address, header, 34
             * bytes, CRC, at 1 Mbit) and a margin for the receiver's ramp up.
             */
            constexpr std::uint32_t inter_frame_space_us    = 150;
            constexpr std::uint32_t longest_response_us     = ( 1 + 4 + 2 + 34 + 3 ) * 8;
            constexpr std::uint32_t response_window_us      = inter_frame_space_us + longest_response_us + 50;

            constexpr std::uint32_t advertising_access_address  = 0x8E89BED6;
            constexpr std::uint32_t advertising_crc_init        = 0x555555;

            /*
             * The Core Specification's channel to frequency mapping, in MHz above 2400.
             */
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

            /*
             * Time on air of a legacy PDU at 1 Mbit: preamble, access address, header,
             * payload and CRC, eight microseconds per byte.
             */
            std::uint32_t air_time_us( std::uint32_t payload_size )
            {
                return ( 1 + 4 + 2 + payload_size + 3 ) * 8;
            }

            void configure_timer( NRF_TIMER_Type& timer )
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

            void start_timers_together()
            {
                NRF_PPI->CH[ ppi_start_timers ].EEP = reinterpret_cast< std::uint32_t >( &NRF_EGU0->EVENTS_TRIGGERED[ 0 ] );
                NRF_PPI->CH[ ppi_start_timers ].TEP = reinterpret_cast< std::uint32_t >( &NRF_TIMER0->TASKS_START );
                NRF_PPI->FORK[ ppi_start_timers ].TEP = reinterpret_cast< std::uint32_t >( &NRF_TIMER1->TASKS_START );
                NRF_PPI->CHENSET = 1u << ppi_start_timers;

                NRF_EGU0->EVENTS_TRIGGERED[ 0 ] = 0;
                NRF_EGU0->TASKS_TRIGGER[ 0 ]    = 1;

                NRF_PPI->CHENCLR = 1u << ppi_start_timers;
                NRF_EGU0->EVENTS_TRIGGERED[ 0 ] = 0;
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

        radio_base* radio_base::instance_ = nullptr;

        radio_base::radio_base()
            : state_( state::idle )
            , receive_{ nullptr, 0 }
            , transmit_time_()
            , ready_pending_( true )
            , radio_event_pending_( false )
            , radio_event_( event::adv_timeout )
            , radio_event_time_()
            , received_size_( 0 )
            , timer_scheduled_( false )
            , timer_when_()
            , timer_event_pending_( false )
        {
            assert( instance_ == nullptr );
            instance_ = this;

            NRF_CLOCK->EVENTS_HFCLKSTARTED  = 0;
            NRF_CLOCK->TASKS_HFCLKSTART     = 1;
            while ( !NRF_CLOCK->EVENTS_HFCLKSTARTED )
                ;

            configure_timer( *NRF_TIMER0 );
            configure_timer( *NRF_TIMER1 );
            start_timers_together();

            configure_radio();
            set_access_address_and_crc_init( advertising_access_address, advertising_crc_init );

            // bias correction on, one value per start; the toolbox starts it per value it draws
            NRF_RNG->CONFIG = RNG_CONFIG_DERCEN_Msk;
            NRF_RNG->SHORTS = RNG_SHORTS_VALRDY_STOP_Msk;

            NVIC_SetPriority( RADIO_IRQn, 0 );
            NVIC_ClearPendingIRQ( RADIO_IRQn );
            NVIC_EnableIRQ( RADIO_IRQn );

            NVIC_SetPriority( TIMER1_IRQn, 1 );
            NVIC_ClearPendingIRQ( TIMER1_IRQn );
            NVIC_EnableIRQ( TIMER1_IRQn );
        }

        void radio_base::sleep()
        {
            __WFE();
        }

        void radio_base::wake_up()
        {
            __SEV();
        }

        link_layer::abs_time radio_base::now() const
        {
            NRF_TIMER0->TASKS_CAPTURE[ cc_now ] = 1;

            return link_layer::abs_time( NRF_TIMER0->CC[ cc_now ] );
        }

        void radio_base::set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
        {
            NRF_RADIO->BASE0    = access_address << 8;
            NRF_RADIO->PREFIX0  = access_address >> 24;
            NRF_RADIO->CRCINIT  = crc_init;
        }

        bool radio_base::start_advertising(
            std::uint32_t                       channel,
            const link_layer::write_buffer&     transmit,
            const link_layer::write_buffer&,
            const link_layer::read_buffer&      receive )
        {
            return schedule( channel, now() + link_layer::delta_time::usec( earliest_us ), transmit, receive );
        }

        bool radio_base::schedule_advertising_event(
            std::uint32_t                       channel,
            link_layer::abs_time                when,
            const link_layer::write_buffer&     transmit,
            const link_layer::write_buffer&,
            const link_layer::read_buffer&      receive )
        {
            if ( when.is_in_near_past( now() + link_layer::delta_time::usec( earliest_us ) ) )
                return false;

            return schedule( channel, when, transmit, receive );
        }

        /*
         * The transmission starts when TIMER0 reaches compare 0, through PPI into TXEN, a
         * ramp up before the first bit is due; the end of the packet lands in capture 2
         * through PPI, and the shorts take the radio from there into receiving. The
         * interrupt on DISABLED then finishes the setup of the receiver.
         */
        bool radio_base::schedule(
            std::uint32_t                       channel,
            link_layer::abs_time                when,
            const link_layer::write_buffer&     transmit,
            const link_layer::read_buffer&      receive )
        {
            assert( receive.buffer && receive.size >= 2 );
            assert( transmit.buffer && transmit.size >= 2 );

            if ( state_ != state::idle )
                return false;

            receive_        = receive;
            transmit_time_  = when;

            NRF_RADIO->FREQUENCY    = frequency_from_channel( channel );
            NRF_RADIO->DATAWHITEIV  = channel & 0x3f;
            NRF_RADIO->PACKETPTR    = reinterpret_cast< std::uint32_t >( transmit.buffer );
            NRF_RADIO->PCNF1        = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( ( transmit.size - 2 ) << RADIO_PCNF1_MAXLEN_Pos );
            NRF_RADIO->SHORTS       =
                  RADIO_SHORTS_READY_START_Msk
                | RADIO_SHORTS_END_DISABLE_Msk
                | RADIO_SHORTS_DISABLED_RXEN_Msk;

            NRF_RADIO->EVENTS_READY     = 0;
            NRF_RADIO->EVENTS_ADDRESS   = 0;
            NRF_RADIO->EVENTS_END       = 0;
            NRF_RADIO->EVENTS_DISABLED  = 0;

            NRF_TIMER0->EVENTS_COMPARE[ cc_start ]      = 0;
            NRF_TIMER0->EVENTS_COMPARE[ cc_window_end ] = 0;
            NRF_TIMER0->CC[ cc_start ]                  = when.data() - ramp_up_us;

            NRF_PPI->CHENCLR = ppi_compare0_rxen | ppi_compare1_disable;
            NRF_PPI->CHENSET = ppi_compare0_txen | ppi_end_capture2;

            state_ = state::transmitting;

            NRF_RADIO->INTENSET = RADIO_INTENSET_DISABLED_Msk;

            return true;
        }

        /*
         * Definitive against the start of the event: once the PPI channel that starts
         * the transmission is off, the radio is either still disabled, in which case the
         * start can no longer happen, or it has begun to ramp up, in which case the event
         * proceeds. The two reads a microsecond apart cover the cycle the task takes.
         */
        bool radio_base::cancel_radio_event()
        {
            if ( state_ != state::transmitting )
                return false;

            NRF_PPI->CHENCLR = ppi_compare0_txen;

            const link_layer::abs_time start = now();
            while ( now().data() - start.data() < 2 )
                ;

            if ( ( NRF_RADIO->STATE & RADIO_STATE_STATE_Msk ) != ( RADIO_STATE_STATE_Disabled << RADIO_STATE_STATE_Pos ) )
                return false;

            NRF_RADIO->INTENCLR = RADIO_INTENCLR_DISABLED_Msk;
            NRF_RADIO->SHORTS   = 0;
            NRF_PPI->CHENCLR    = ppi_end_capture2;

            state_ = state::idle;

            return true;
        }

        bool radio_base::schedule_timer( link_layer::abs_time when )
        {
            assert( !timer_scheduled_ );

            if ( when.is_in_near_past( now() + link_layer::delta_time::usec( 2 ) ) )
                return false;

            timer_when_      = when;
            timer_scheduled_ = true;

            NRF_TIMER1->EVENTS_COMPARE[ cc_user_timer ] = 0;
            NRF_TIMER1->CC[ cc_user_timer ]             = when.data();
            NRF_TIMER1->INTENSET = TIMER_INTENSET_COMPARE0_Msk;

            return true;
        }

        /*
         * The compare event is set by the hardware whether or not it interrupts, so once
         * the interrupt is off, the event tells whether the timer expired in the meantime;
         * if it did, the interrupt is turned on again and delivers it.
         */
        bool radio_base::cancel_timer()
        {
            if ( !timer_scheduled_ )
                return false;

            NRF_TIMER1->INTENCLR = TIMER_INTENCLR_COMPARE0_Msk;

            if ( NRF_TIMER1->EVENTS_COMPARE[ cc_user_timer ] || timer_event_pending_ )
            {
                NRF_TIMER1->INTENSET = TIMER_INTENSET_COMPARE0_Msk;

                return false;
            }

            timer_scheduled_ = false;

            return true;
        }

        std::optional< radio_base::happened > radio_base::next_event()
        {
            if ( ready_pending_ )
            {
                ready_pending_ = false;

                return happened{ event::radio_ready, link_layer::abs_time(), { nullptr, 0 } };
            }

            if ( radio_event_pending_ )
            {
                radio_event_pending_ = false;

                return happened{ radio_event_, radio_event_time_, { receive_.buffer, received_size_ } };
            }

            if ( timer_event_pending_ )
            {
                timer_event_pending_ = false;

                return happened{ event::user_timer, timer_when_, { nullptr, 0 } };
            }

            return std::nullopt;
        }

        void radio_base::on_radio_disabled()
        {
            NRF_RADIO->EVENTS_DISABLED = 0;

            if ( state_ == state::transmitting )
            {
                // the short is ramping the receiver up already; it gets the buffer and its deadline
                NRF_RADIO->EVENTS_END   = 0;
                NRF_RADIO->PACKETPTR    = reinterpret_cast< std::uint32_t >( receive_.buffer );
                NRF_RADIO->PCNF1        = ( NRF_RADIO->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( ( receive_.size - 2 ) << RADIO_PCNF1_MAXLEN_Pos );
                NRF_RADIO->SHORTS       = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;

                NRF_TIMER0->EVENTS_COMPARE[ cc_window_end ] = 0;
                NRF_TIMER0->CC[ cc_window_end ]             = NRF_TIMER0->CC[ cc_packet_end ] + response_window_us;
                NRF_PPI->CHENSET = ppi_compare1_disable;

                state_ = state::receiving;
            }
            else if ( state_ == state::receiving )
            {
                NRF_PPI->CHENCLR    = ppi_compare0_txen | ppi_compare1_disable | ppi_end_capture2;
                NRF_RADIO->SHORTS   = 0;
                NRF_RADIO->INTENCLR = RADIO_INTENCLR_DISABLED_Msk;

                const bool crc_ok = ( NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) == ( RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos );

                if ( NRF_RADIO->EVENTS_END && crc_ok )
                {
                    const std::uint32_t payload_size = receive_.buffer[ 1 ];

                    received_size_    = std::min< std::size_t >( payload_size + 2, receive_.size );
                    radio_event_time_ = link_layer::abs_time( NRF_TIMER0->CC[ cc_packet_end ] - air_time_us( payload_size ) );
                    radio_event_      = event::adv_received;
                }
                else
                {
                    radio_event_time_ = transmit_time_;
                    radio_event_      = event::adv_timeout;
                }

                state_               = state::idle;
                radio_event_pending_ = true;
                __SEV();
            }
        }

        void radio_base::on_timer_expired()
        {
            NRF_TIMER1->EVENTS_COMPARE[ cc_user_timer ] = 0;
            NRF_TIMER1->INTENCLR = TIMER_INTENCLR_COMPARE0_Msk;

            timer_scheduled_     = false;
            timer_event_pending_ = true;
            __SEV();
        }

        void radio_base::radio_interrupt()
        {
            if ( instance_ )
                instance_->on_radio_disabled();
        }

        void radio_base::timer_interrupt()
        {
            if ( instance_ )
                instance_->on_timer_expired();
        }
    }
}

extern "C" void RADIO_IRQHandler()
{
    bluetoe::nrf52_details::radio_base::radio_interrupt();
}

extern "C" void TIMER1_IRQHandler()
{
    bluetoe::nrf52_details::radio_base::timer_interrupt();
}
