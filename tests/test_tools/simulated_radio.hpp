#ifndef BLUETOE_TESTS_TEST_TOOLS_SIMULATED_RADIO_HPP
#define BLUETOE_TESTS_TEST_TOOLS_SIMULATED_RADIO_HPP

/**
 * @file simulated_radio.hpp
 *
 * The simulated radio on the interface of <bluetoe/scheduled_radio2.hpp>: the same
 * simulation and the same test facing API as test_radio.hpp, whose radio_base and recorded
 * types it shares, on the interface the link layer is moving to.
 *
 * What differs is what the interface changed: every time is an abs_time rather than a delta
 * from the last event, the link layer owns the PDU buffer and the radio asks it for the one
 * of the current connection, advertising begins with start_advertising_event() and continues at a
 * time, a connection event names its start and its end rather than a window and an interval,
 * and what a radio filters by is an acceptance filter rather than a white list.
 *
 * What a test sees is unchanged: the recorded times stay delta_time, the schedule times
 * counted from the start of the simulation and the windows from the moment of scheduling, so
 * that the expectations of the existing tests keep their meaning.
 */

#include "test_radio.hpp"

#include <bluetoe/scheduled_radio2.hpp>
#include <bluetoe/abs_time.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace test {

    /**
     * @brief the time the simulation starts at
     *
     * Not zero, so that a time before the first anchor is still a time and not a wrap around.
     */
    static constexpr bluetoe::link_layer::abs_time::representation_type simulation_start_us = 0x10000;

    /**
     * @brief simulated radio on the scheduled_radio2 interface
     *
     * CallBack is what the radio delivers to, the link layer: it provides the callbacks and,
     * through link_layer_pdu_buffer(), the PDU buffer of the current connection.
     */
    template <
        typename CallBack,
        bool Phy2MBitSupported = true,
        bool SynchronizedUserTimerSupported = true >
    class simulated_radio : public radio_base
    {
    public:
        simulated_radio();

        /** @name the scheduled_radio interface
         * @{
         */
        static constexpr bool           hardware_supports_encryption                = false;
        static constexpr bool           hardware_supports_lesc_pairing              = false;
        static constexpr bool           hardware_supports_legacy_pairing            = false;
        static constexpr bool           hardware_supports_2mbit                     = Phy2MBitSupported;
        static constexpr bool           hardware_supports_synchronized_user_timer   = SynchronizedUserTimerSupported;
        static constexpr bool           hardware_supports_link_layer_context        = false;
        static constexpr std::size_t    radio_package_overhead                      = 0;
        static constexpr std::uint32_t  radio_max_supported_payload_length          = 251;
        static constexpr std::uint32_t  sleep_time_accuracy_ppm                     = 50;
        static constexpr std::size_t    radio_maximum_acceptance_filter_entries     = 0;

        /**
         * @brief what a connection event needs to be set up, from the moment it is scheduled
         *
         * schedule_connection_event() refuses a time closer than this, as hardware does.
         */
        static constexpr unsigned       connection_event_setup_time_us              = 100;

        struct ccm_counter_t {};

        // the simulation runs in one context, so neither lock excludes anything
        struct radio_lock_guard {};
        struct link_layer_lock_guard {};

        void run();
        void wake_up();

        void set_ccm_counter( const ccm_counter_t&, const ccm_counter_t& ) {}

        void set_phy(
            bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t receiving,
            bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t transmitting );

        void set_local_address( const bluetoe::link_layer::device_address& address );

        void start_advertising_event(
            std::uint32_t                               channel,
            const bluetoe::link_layer::write_buffer&    transmit,
            const bluetoe::link_layer::write_buffer&    response,
            const bluetoe::link_layer::read_buffer&     receive );

        bool schedule_advertising_event(
            std::uint32_t                               channel,
            bluetoe::link_layer::abs_time               when,
            const bluetoe::link_layer::write_buffer&    transmit,
            const bluetoe::link_layer::write_buffer&    response,
            const bluetoe::link_layer::read_buffer&     receive );

        bool schedule_connection_event(
            std::uint32_t                               channel,
            bluetoe::link_layer::abs_time               start_receive,
            bluetoe::link_layer::abs_time               end_receive );

        bool cancel_radio_event();

        bool schedule_timer( bluetoe::link_layer::abs_time when );
        bool cancel_timer();
        /** @} */

        /** @name what a test asks of the simulation, beside what radio_base offers
         * @{
         */

        /**
         * @brief the time the simulation stands at
         */
        bluetoe::link_layer::abs_time now() const;

        /**
         * @brief the anchor of the connection event simulated last
         */
        bluetoe::link_layer::abs_time last_anchor() const;

        /**
         * @brief how many scheduled events were cancelled before they were simulated
         */
        unsigned cancelled_events() const;
        /** @} */

    private:
        using radio_t = simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >;
        using layout  = typename bluetoe::link_layer::pdu_layout_by_radio< radio_t >::pdu_layout;

        CallBack& deliver_to()
        {
            return static_cast< CallBack& >( *this );
        }

        // the PDU buffer of the current connection, which the radio touches during an event only
        auto& connection_buffer()
        {
            return deliver_to().link_layer_pdu_buffer();
        }

        // a time as a test reads it: counted from the start of the simulation
        static bluetoe::link_layer::delta_time since_start( bluetoe::link_layer::abs_time when )
        {
            return when - bluetoe::link_layer::abs_time( simulation_start_us );
        }

        void record_advertising(
            std::uint32_t                               channel,
            bluetoe::link_layer::abs_time               when,
            const bluetoe::link_layer::write_buffer&    transmit,
            const bluetoe::link_layer::read_buffer&     receive );

        void simulate_advertising_response();
        void simulate_connection_event_response();

        bluetoe::link_layer::abs_time simulate_user_timer_response( bluetoe::link_layer::abs_time end );

        void copy_air_to_memory( const std::vector< std::uint8_t >& over_the_air, bluetoe::link_layer::read_buffer& in_memory );
        std::vector< std::uint8_t > memory_to_air( bluetoe::link_layer::write_buffer );

        bluetoe::link_layer::abs_time   now_;
        bluetoe::link_layer::abs_time   last_anchor_;

        // exactly one action is scheduled at a time; which one is to be simulated next
        bool                            idle_;
        bool                            advertising_response_;
        bool                            connection_event_response_;
        int                             wake_ups_;
        unsigned                        cancelled_events_;
        bool                            radio_ready_pending_;

        bool                            timer_set_;
        bluetoe::link_layer::abs_time   timer_at_;

        bluetoe::link_layer::device_address local_address_;

    protected:
        bool reception_encrypted_;
        bool transmition_encrypted_;
    };

    // implementation
    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::simulated_radio()
        : now_( bluetoe::link_layer::abs_time( simulation_start_us ) )
        , last_anchor_( bluetoe::link_layer::abs_time( simulation_start_us ) )
        , idle_( true )
        , advertising_response_( false )
        , connection_event_response_( false )
        , wake_ups_( 0 )
        , cancelled_events_( 0 )
        , radio_ready_pending_( true )
        , timer_set_( false )
        , reception_encrypted_( false )
        , transmition_encrypted_( false )
    {
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    bluetoe::link_layer::abs_time simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::now() const
    {
        return now_;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    bluetoe::link_layer::abs_time simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::last_anchor() const
    {
        return last_anchor_;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    unsigned simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::cancelled_events() const
    {
        return cancelled_events_;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::set_phy(
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t receiving,
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t transmitting )
    {
        this->radio_set_phy( receiving, transmitting );
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::set_local_address(
        const bluetoe::link_layer::device_address& address )
    {
        local_address_ = address;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::record_advertising(
        std::uint32_t                               channel,
        bluetoe::link_layer::abs_time               when,
        const bluetoe::link_layer::write_buffer&    transmit,
        const bluetoe::link_layer::read_buffer&     receive )
    {
        assert( idle_ );
        assert( access_address_and_crc_valid_ );
        assert( transmit.buffer );

        idle_                       = false;
        advertising_response_       = true;
        connection_event_response_  = false;

        const advertising_data data{
            since_start( now_ ),
            since_start( when ),
            channel,
            when - now_,
            memory_to_air( transmit ),
            receive,
            access_address_,
            crc_init_
        };

        advertised_data_.push_back( data );
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::start_advertising_event(
        std::uint32_t                               channel,
        const bluetoe::link_layer::write_buffer&    transmit,
        const bluetoe::link_layer::write_buffer&    /* response */,
        const bluetoe::link_layer::read_buffer&     receive )
    {
        // as soon as possible, which the simulation can make immediate
        record_advertising( channel, now_, transmit, receive );
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    bool simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::schedule_advertising_event(
        std::uint32_t                               channel,
        bluetoe::link_layer::abs_time               when,
        const bluetoe::link_layer::write_buffer&    transmit,
        const bluetoe::link_layer::write_buffer&    /* response */,
        const bluetoe::link_layer::read_buffer&     receive )
    {
        if ( when.is_in_near_past( now_ ) )
            return false;

        record_advertising( channel, when, transmit, receive );

        return true;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    bool simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::schedule_connection_event(
        std::uint32_t                   channel,
        bluetoe::link_layer::abs_time   start_receive,
        bluetoe::link_layer::abs_time   end_receive )
    {
        // as the hardware does: a time that has gone by, or is too close to set the radio up
        if ( start_receive.is_in_near_past( now_ + bluetoe::link_layer::delta_time( connection_event_setup_time_us ) ) )
            return false;

        advertising_response_       = false;
        connection_event_response_  = true;
        idle_                       = false;

        // recorded from the last anchor, which is what the link layer measures the window from
        const connection_event data{
            since_start( now_ ),
            channel,
            start_receive - last_anchor_,
            end_receive - last_anchor_,
            receiving_encoding_,
            transmiting_encoding_,
            access_address_,
            crc_init_,
            pdu_list_t(),
            pdu_list_t(),
            reception_encrypted_,
            transmition_encrypted_
        };

        connection_events_.push_back( data );

        return true;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    bool simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::cancel_radio_event()
    {
        if ( idle_ )
            return false;

        if ( connection_event_response_ )
        {
            assert( !connection_events_.empty() );
            connection_events_.pop_back();
            connection_event_response_ = false;
        }
        else if ( advertising_response_ )
        {
            assert( !advertised_data_.empty() );
            advertised_data_.pop_back();
            advertising_response_ = false;
        }

        idle_ = true;
        ++cancelled_events_;

        return true;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    bool simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::schedule_timer(
        bluetoe::link_layer::abs_time when )
    {
        assert( !timer_set_ );

        if ( when.is_in_near_past( now_ ) )
            return false;

        const scheduled_user_timer new_timer = {
            since_start( now_ ),
            since_start( last_anchor_ ),
            when - last_anchor_ };

        scheduled_user_timers_.push_back( new_timer );

        timer_set_ = true;
        timer_at_  = when;

        return true;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    bool simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::cancel_timer()
    {
        const bool result = timer_set_;

        if ( timer_set_ )
        {
            timer_set_ = false;
            scheduled_user_timers_.pop_back();
        }

        return result;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::wake_up()
    {
        ++wake_ups_;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::run()
    {
        if ( radio_ready_pending_ )
        {
            radio_ready_pending_ = false;
            deliver_to().radio_ready();
        }

        bool new_scheduling_added = false;

        do
        {
            const unsigned count = advertised_data_.size() + connection_events_.size();

            if ( advertising_response_ )
            {
                advertising_response_ = false;
                simulate_advertising_response();
            }
            else if ( connection_event_response_ )
            {
                connection_event_response_ = false;
                simulate_connection_event_response();
            }

            // there should be at max one call to a schedule function
            assert( count + 1 >= advertised_data_.size() + connection_events_.size() );

            new_scheduling_added = advertised_data_.size() + connection_events_.size() > count;
        } while ( since_start( now_ ) < eos_ && new_scheduling_added && wake_ups_ == 0 );

        if ( wake_ups_ )
            --wake_ups_;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::simulate_advertising_response()
    {
        assert( !advertised_data_.empty() );

        advertising_data&                       current  = advertised_data_.back();
        std::pair< bool, advertising_response > response = find_response( current );

        // the advertising was scheduled for a time; the simulation goes there
        now_ += current.transmision_time;

        if ( response.first )
        {
            now_ += T_IFS;

            if ( response.second.has_crc_error )
            {
                idle_ = true;
                deliver_to().adv_timeout( now_ );
            }
            else
            {
                if ( current.receive_buffer.size > 0 )
                    copy_air_to_memory( response.second.received_data, current.receive_buffer );

                last_anchor_                = now_;
                central_sequence_number_    = 0;
                central_ne_sequence_number_ = 0;

                idle_ = true;
                deliver_to().adv_received( now_, current.receive_buffer );
            }
        }
        else
        {
            idle_ = true;
            deliver_to().adv_timeout( now_ );
        }
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::simulate_connection_event_response()
    {
        connection_event_response response = connection_events_response_.empty()
            ? connection_event_response()
            : connection_events_response_.front();

        assert( !connection_events_.empty() );
        auto& event = connection_events_.back();

        if ( !connection_events_response_.empty() )
            connection_events_response_.erase( connection_events_response_.begin() );

        const auto window_start = last_anchor_ + event.start_receive;
        const auto window_end   = last_anchor_ + event.end_receive;

        if ( response.timeout )
        {
            now_  = simulate_user_timer_response( window_end );
            idle_ = true;

            deliver_to().connection_timeout( window_end );
        }
        else
        {
            // timers that expire before the event are delivered while the anchor is still the last one
            now_         = simulate_user_timer_response( window_start );
            last_anchor_ = window_start;

            static constexpr std::uint8_t sn_flag        = 0x8;
            static constexpr std::uint8_t nesn_flag      = 0x4;
            static constexpr std::uint8_t more_data_flag = 0x10;

            bool more_data = false;

            pdu_list_t pdus = response.data;

            if ( pdus.empty() && response.func )
                pdus = response.func();

            bluetoe::link_layer::connection_event_events events;

            do
            {
                auto receive_buffer = connection_buffer().allocate_receive_buffer();

                more_data = false;

                if ( receive_buffer.size )
                {
                    // what is the link layer going to receive?
                    if ( pdus.empty() )
                    {
                        layout::header( receive_buffer.buffer, 0x0001 );
                    }
                    else
                    {
                        const auto pdu = pdus.front();
                        pdus.erase( pdus.begin() );

                        copy_air_to_memory( pdu.data, receive_buffer );

                        more_data = !pdus.empty();
                    }

                    std::uint16_t header = layout::header( receive_buffer );
                    header &= ~( sn_flag | nesn_flag );
                    header |= central_sequence_number_ | central_ne_sequence_number_;
                    layout::header( receive_buffer, header );

                    central_sequence_number_ ^= sn_flag;
                }

                if ( more_data && receive_buffer.size )
                {
                    const std::uint16_t header = layout::header( receive_buffer ) | more_data_flag;
                    layout::header( receive_buffer, header );
                }

                // the buffer also reports what the encryption's counters would have to do
                auto answer = connection_buffer().received( receive_buffer ).transmit;

                more_data = more_data || ( layout::header( answer ) & more_data_flag );
                central_ne_sequence_number_ ^= nesn_flag;

                event.received_data.push_back(
                    memory_to_air( bluetoe::link_layer::write_buffer( receive_buffer ) ) );

                event.transmitted_data.push_back(
                    pdu_t( memory_to_air( answer ), transmition_encrypted_ ) );

            } while ( more_data );

            idle_ = true;

            deliver_to().connection_end_event( last_anchor_, events );
        }
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    bluetoe::link_layer::abs_time simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::simulate_user_timer_response(
        bluetoe::link_layer::abs_time end )
    {
        while ( timer_set_ && timer_at_.is_in_near_past( end ) )
        {
            now_       = timer_at_;
            timer_set_ = false;

            deliver_to().user_timer( timer_at_ );
        }

        return end;
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    void simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::copy_air_to_memory(
        const std::vector< std::uint8_t >& over_the_air, bluetoe::link_layer::read_buffer& in_memory )
    {
        const std::uint16_t header = bluetoe::details::read_16bit( over_the_air.data() );
        const std::size_t   size   = std::min< std::size_t >( header >> 8, over_the_air.size() - ll_header_size );
        const auto          body   = layout::body( in_memory );

        layout::header( in_memory, header );
        std::copy( over_the_air.data() + ll_header_size, over_the_air.data() + ll_header_size + size, body.first );

        in_memory.size = layout::data_channel_pdu_memory_size( size );
    }

    template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
    std::vector< std::uint8_t > simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported >::memory_to_air(
        bluetoe::link_layer::write_buffer memory )
    {
        const std::uint16_t header    = layout::header( memory );
        const auto          body      = layout::body( memory );
        const std::size_t   body_size = header >> 8;

        std::vector< std::uint8_t > air( body_size + ll_header_size );
        bluetoe::details::write_16bit( &air[ 0 ], header );
        std::copy( body.first, body.first + body_size, &air[ ll_header_size ] );

        return air;
    }
}

namespace test {
    /*
     * The radios the link layer tests are written against.
     */
    template < class CallBack >
    using radio = simulated_radio< CallBack, false, false >;

    template < class CallBack >
    using radio_no_2mbit = simulated_radio< CallBack, false, false >;

    template < class CallBack >
    using radio_with_2mbit = simulated_radio< CallBack, true, false >;

    template < class CallBack >
    using radio_with_user_timer = simulated_radio< CallBack, false, true >;

    template < class CallBack >
    using radio_without_user_timer = simulated_radio< CallBack, false, false >;

    /*
     * A radio that answers the link layer's encryption calls, as far as the tests need
     * them, until the scheduled radio 2 interface says how a connection is encrypted.
     */
    template < class CallBack >
    class radio_with_encryption : public radio< CallBack >
    {
    public:
        static constexpr bool hardware_supports_encryption = true;

        radio_with_encryption()
            : key_( { { 0x00 } } )
            , skdm_( 0u )
            , ivm_( 0u )
            , skds_( 0x3fac22107855aa56ul )
            , ivs_( 0x78563412 )
        {
        }

        // Security functions
        bluetoe::details::uint128_t create_srand()
        {
            const bluetoe::details::uint128_t r{{
                0xE0, 0x2E, 0x70, 0xC6,
                0x4E, 0x27, 0x88, 0x63,
                0x0E, 0x6F, 0xAD, 0x56,
                0x21, 0xD5, 0x83, 0x57
            }};

            return r;
        }

        bluetoe::details::uint128_t c1(
            const bluetoe::details::uint128_t& temp_key,
            const bluetoe::details::uint128_t& /* srand */,
            const bluetoe::details::uint128_t& /* p1 */,
            const bluetoe::details::uint128_t& /* p2 */ ) const
        {
            return temp_key;
        }

        bluetoe::details::uint128_t s1(
            const bluetoe::details::uint128_t& stk,
            const bluetoe::details::uint128_t& /* srand */,
            const bluetoe::details::uint128_t& /* mrand */)
        {
            return stk;
        }

        void setup_encryption_response( std::uint64_t SKDs, std::uint32_t IVs)
        {
            skds_ = SKDs;
            ivs_  = IVs;
        }

        std::pair< std::uint64_t, std::uint32_t > setup_encryption( bluetoe::details::uint128_t k, std::uint64_t skdm, std::uint32_t ivm )
        {
            skdm_ = skdm;
            ivm_  = ivm;

            key_ = k;

            return { skds_, ivs_ };
        }

        void start_receive_encrypted()
        {
            this->reception_encrypted_ = true;
        }

        void start_transmit_encrypted()
        {
            this->transmition_encrypted_ = true;
        }

        void stop_receive_encrypted()
        {
            this->reception_encrypted_ = false;
        }

        void stop_transmit_encrypted()
        {
            this->transmition_encrypted_ = false;
        }

        // access to data provided for testing
        bluetoe::details::uint128_t encryption_key() const
        {
            return key_;
        }

        std::uint64_t skdm() const
        {
            return skdm_;
        }

        std::uint32_t ivm() const
        {
            return ivm_;
        }

    private:
        bluetoe::details::uint128_t key_;
        std::uint64_t               skdm_;
        std::uint32_t               ivm_;
        std::uint64_t               skds_;
        std::uint32_t               ivs_;
    };

    // implementation
}

/*
 * Header inverted and a gap of two octets between header and payload, so that every part of
 * the library has to take the layout into account.
 */
namespace bluetoe {
    namespace link_layer {

        template < typename CallBack, bool Phy2MBitSupported, bool SynchronizedUserTimerSupported >
        struct pdu_layout_by_radio< test::simulated_radio< CallBack, Phy2MBitSupported, SynchronizedUserTimerSupported > >
        {
            using pdu_layout = test::pdu_layout;
        };

        template < typename CallBack >
        struct pdu_layout_by_radio< test::radio_with_encryption< CallBack > >
        {
            using pdu_layout = test::pdu_layout;
        };
    }
}

#endif
