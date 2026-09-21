#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_DUT_RIG_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_DUT_RIG_HPP

/**
 * @file dut_rig.hpp
 *
 * The platform independent half of a device under test: everything that is neither the
 * radio nor the serial port, on top of what instrument/instrument.hpp provides to both
 * instruments.
 *
 * The rig is a template over the scheduled radio implementation and over the platform's
 * serial port; dut_rigs/template_dut_rig.cpp shows how a platform binds the two. The
 * host instantiates the same template with dummies (host/dut_functions.hpp) to obtain the function
 * list the wire is keyed on, which is why no function of the list carries a type of the
 * radio.
 *
 * The function list is the instrument functions, the program and its records, and the
 * pairing toolbox, the same on every device. On a radio without a toolbox the toolbox
 * opcodes are answered with status::unsupported_function, and the host reads properties()
 * before it asks.
 *
 * The rig is the device's program interpreter: a step waits for the callback it
 * names and runs inside it, and everything the radio reports or the rig calls is recorded
 * in one queue, in the order it happened.
 */

#include "instrument/address_set.hpp"
#include "instrument/instrument.hpp"
#include "instrument/reported_queue.hpp"
#include "link/frame.hpp"
#include "link/function_list.hpp"
#include "link/program.hpp"

#include <bluetoe/ll_data_pdu_buffer.hpp>
#include <bluetoe/scheduled_radio2.hpp>
#include <bluetoe/radio_properties.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief version of the wire protocol of the device under test
     *
     * Counts the function lists a firmware was built with: changes whenever
     * dut_rig::functions changes after a device was flashed with the current one.
     */
    constexpr std::uint16_t dut_protocol_version = 2;

    /**
     * @brief records the rig keeps until the host collects them
     *
     * A program's worth: a callback and a call or two per step, a few steps, and room
     * for what a test provokes on purpose to see the queue overflow.
     */
    constexpr std::size_t record_queue_size = 32;

    /**
     * @brief addresses the acceptance filter of the rig holds
     *
     * A test accepts the tester, and perhaps a second sender; more is not needed. The
     * reset before every test empties the set, as it does the program.
     */
    constexpr std::size_t max_acceptance_filter_entries = 4;

    /**
     * @brief how many PDUs of the largest size the receive side of the PDU buffer holds
     *        before it has no room, and the transmit side takes
     */
    constexpr std::size_t received_pdus_until_full = 4;

    /**
     * @brief bytes of the PDU buffer of connection events, for each direction
     *
     * The rig is a link layer that negotiated the largest PDU, so the buffer reserves that
     * much room per PDU whatever a PDU carries; this many of them fit.
     */
    constexpr std::size_t pdu_buffer_size = received_pdus_until_full * max_data_pdu_size;

    /**
     * @brief the PDU buffers of the rig, one for each connection a test runs on the radio
     */
    constexpr std::size_t pdu_buffer_count = 2;

    /**
     * @brief data channel PDUs the calls of a program queue, all its steps together
     *
     * A call keeps only the place of its PDU here, so that the room of a PDU of the largest
     * payload is reserved once per queue_pdu of the program and not once per call.
     */
    constexpr std::size_t max_queued_pdus = 4;

    /**
     * @brief PDUs a program's read_received() takes out of the PDU buffer and the rig keeps until
     *        the host collects them; what does not fit is dropped
     */
    constexpr std::size_t read_queue_size = 2 * received_pdus_until_full;

    namespace details {

        /*
         * The toolbox as the wire sees it, for a rig around a radio that has none. The
         * list names these functions at the toolbox's positions, so that it is the same
         * as on a device with a toolbox; no object of the rig's dispatcher is of this
         * class, so the dispatcher answers status::unsupported_function and never calls
         * them. The bodies exist only because a member pointer needs a definition.
         */
        class no_toolbox
        {
        public:
            using coordinate_t = std::array< std::uint8_t, 32 >;

            std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > generate_keys()
            {
                return {};
            }

            bluetoe::details::uint128_t select_random_nonce()
            {
                return {};
            }

            bluetoe::details::ecdh_shared_secret_t p256(
                const bluetoe::details::ecdh_private_key_t&, const bluetoe::details::ecdh_public_key_t& )
            {
                return {};
            }

            bluetoe::details::uint128_t f4(
                const coordinate_t&, const coordinate_t&, const bluetoe::details::uint128_t&, std::uint8_t )
            {
                return {};
            }

            std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > f5(
                const bluetoe::details::ecdh_shared_secret_t&,
                const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t&,
                const link_layer::device_address&, const link_layer::device_address& )
            {
                return {};
            }

            bluetoe::details::uint128_t f6(
                const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t&,
                const bluetoe::details::uint128_t&, const bluetoe::details::io_capabilities_t&,
                const link_layer::device_address&, const link_layer::device_address& )
            {
                return {};
            }

            std::uint32_t g2(
                const coordinate_t&, const coordinate_t&,
                const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t& )
            {
                return 0;
            }
        };
    }

    /**
     * @brief the rig around a scheduled radio implementation
     *
     * Radio is the implementation under test, a template over the type that receives its
     * callbacks; a platform binds the implementation's options with an alias template
     * and passes that. The rig is that type: it derives from the radio and passes itself,
     * as the link layer does, and it receives the callbacks in the link layer context.
     * Port is the platform's serial port, a template over the buffer type
     * and the type it wakes, which is the rig, whose wake_up() is the radio's. MaxPayload
     * bounds a request and a response, and thereby the buffers.
     *
     * The concept is checked in the constructor rather than in the declaration, because
     * a class cannot name itself in its own constraint.
     */
    template <
        template < typename CallBacks > class Radio,
        template < typename Buffer, typename Wake > class Port,
        std::size_t MaxPayload = default_max_payload >
    class dut_rig
        : public Radio< dut_rig< Radio, Port, MaxPayload > >
        , public instrument< dut_rig< Radio, Port, MaxPayload >, Port, MaxPayload >
    {
    public:
        using radio_t      = Radio< dut_rig >;
        using instrument_t = instrument< dut_rig, Port, MaxPayload >;

        /**
         * @brief whose toolbox functions the list names
         *
         * The radio's, if it has a toolbox; the rig's wrappers stand in for the three
         * with pointer parameters. Without a toolbox, those of details::no_toolbox, which
         * the dispatcher answers with status::unsupported_function.
         */
        using toolbox_t = std::conditional_t< radio_t::hardware_supports_lesc_pairing, radio_t, details::no_toolbox >;
        using wrapped_t = std::conditional_t< radio_t::hardware_supports_lesc_pairing, dut_rig, details::no_toolbox >;

        dut_rig( std::string_view implementation_name, std::string_view build_identifier )
            : instrument_t( implementation_name, build_identifier )
        {
            static_assert( link_layer::scheduled_radio< Radio, dut_rig > );

            // what a link layer does once it agreed the length of a PDU with its central; the
            // rig agrees the largest, so that a test can send and receive one
            for ( pdu_buffer& buffer : pdu_buffers_ )
            {
                buffer.max_rx_size( max_data_pdu_size );
                buffer.max_tx_size( max_data_pdu_size );
            }

            instrument_t::start();
        }

        /**
         * @brief one iteration of the main loop: answer a buffered request, then let the radio run
         */
        void run()
        {
            instrument_t::serve();
            radio_t::run();
        }

        /**
         * @name Callbacks of the radio
         *
         * Each is recorded and then runs the step that waits for it, if the next step
         * does. The callbacks of connection events come with the connection events.
         * @{
         */
        void radio_ready()
        {
            on_callback( callback_kind::radio_ready, link_layer::abs_time(), {} );
        }

        void adv_received( link_layer::abs_time when, const link_layer::read_buffer& received )
        {
            radio_event_pending_ = false;

            assert( received.size <= max_advertising_pdu_size );
            on_callback( callback_kind::adv_received, when, adv_pdu( std::span< const std::uint8_t >( received.buffer, received.size ) ) );
        }

        void adv_timeout( link_layer::abs_time when )
        {
            radio_event_pending_ = false;
            on_callback( callback_kind::adv_timeout, when, {} );
        }

        void user_timer( link_layer::abs_time when )
        {
            timer_pending_ = false;
            on_callback( callback_kind::user_timer, when, {} );
        }

        void connection_timeout( link_layer::abs_time when )
        {
            radio_event_pending_ = false;
            on_callback( callback_kind::connection_timeout, when, {} );
        }

        void connection_end_event( link_layer::abs_time when, link_layer::connection_event_events events )
        {
            radio_event_pending_ = false;
            on_callback( callback_kind::connection_end_event, when, {}, events );
        }

        /**
         * @brief the PDU buffer of connection events, radio context
         *
         * The active one of the rig's buffers, which a program switches between events.
         */
        auto& link_layer_pdu_buffer()
        {
            return pdu_buffers_[ active_buffer_ ];
        }

        /**
         * @brief the acceptance filter, radio context
         *
         * True when the sender is in the filter set, or the set is empty, which is how an
         * empty set accepts every device (scheduled_radio2.hpp). add_to_acceptance_filter()
         * fills the set; the reset before every test empties it.
         */
        bool is_in_acceptance_filter( const link_layer::device_address& sender ) const
        {
            return acceptance_filter_.empty() || acceptance_filter_.contains( sender );
        }
        /** @} */

        /**
         * @name Instrument functions
         *
         * The functions that are the rig's own, beside those of instrument/instrument.hpp; the
         * names and the session token are the instrument's.
         * @{
         */
        std::uint16_t protocol_version() const
        {
            return dut_protocol_version;
        }

        /**
         * @brief whether the started program ran to its end
         *
         * True once the last step ran and the radio reported the end of whatever that
         * step scheduled, so that nothing is still going to happen on air; false before
         * a program was started.
         */
        bool program_finished() const
        {
            return running_ && cursor_ == step_count_ && !radio_event_pending_ && !timer_pending_;
        }

        link_layer::radio_properties properties() const
        {
            return link_layer::radio_properties( static_cast< const radio_t& >( *this ) );
        }
        /** @} */

        /**
         * @name Program and records
         * @{
         */

        /**
         * @brief appends a step to the program, waiting for `on`; its calls follow with add_call()
         *
         * There is no way to remove one: the device is reset before every test, and the
         * reset is what empties the program. Refused, and not appended, if the program is
         * full, or if the step waits for a callback that cannot trigger one.
         */
        bool add_step( callback_kind on )
        {
            if ( step_count_ == max_steps || on == callback_kind::radio_ready )
                return false;

            steps_[ step_count_ ] = program_step{ .on = on, .first_call = call_count_, .call_count = 0 };
            ++step_count_;

            return true;
        }

        /**
         * @brief appends a call to the step added last
         *
         * The steps share max_calls calls, and the queue_pdu calls share max_queued_pdus PDUs.
         * Refused, and not appended, if no step was added yet, if the program's calls or PDUs
         * are all used, if the step waits for start and the call needs a time, since none
         * exists then, or if an advertising PDU of the call is larger than an advertising
         * channel allows.
         */
        bool add_call( const call& next )
        {
            if ( step_count_ == 0 || call_count_ == max_calls )
                return false;

            program_step& last = steps_[ step_count_ - 1 ];

            if ( last.on == callback_kind::start
              && ( next.kind == call_kind::schedule_advertising_event || next.kind == call_kind::schedule_timer
                || next.kind == call_kind::schedule_connection_event ) )
                return false;

            const bool queues = next.kind == call_kind::queue_pdu;

            if ( queues && program_pdu_count_ == max_queued_pdus )
                return false;

            if ( !queues && next.transmit.size > max_advertising_pdu_size )
                return false;

            stored_call& stored = calls_[ call_count_ ];

            stored = stored_call{
                .kind           = next.kind,
                .channel        = next.channel,
                .delay          = next.delay,
                .end_delay      = next.end_delay,
                .response       = next.response,
                .address        = next.address,
                .access_address = next.access_address,
                .crc_init       = next.crc_init,
                .phy            = next.phy };

            if ( queues )
            {
                program_pdus_[ program_pdu_count_ ] = next.transmit;
                stored.pdu                          = program_pdu_count_;
                ++program_pdu_count_;
            }
            else
            {
                stored.transmit = adv_pdu( next.transmit.span() );
            }

            ++call_count_;
            ++last.call_count;

            return true;
        }

        /**
         * @brief runs the program from its first step
         *
         * A first step that waits for start runs now, inside this call.
         */
        void start_program()
        {
            cursor_  = 0;
            running_ = true;

            run_step_if_waiting_for( callback_kind::start, link_layer::abs_time() );
        }

        /**
         * @brief hands over the oldest records and forgets them
         */
        record_batch collect_records()
        {
            return records_.collect< records_per_batch >();
        }

        /**
         * @brief queues a PDU for transmission in a connection event
         *
         * `data` is a data channel PDU; its SN and NESN bits are the buffer's to set. It goes
         * into the active buffer. False, and nothing queued, if the buffer has no room for it.
         * The reset before every test empties the buffers.
         */
        bool queue_pdu( const pdu& data )
        {
            pdu_buffer&             buffer = link_layer_pdu_buffer();
            link_layer::read_buffer room   = buffer.allocate_transmit_buffer( data.size );

            if ( room.size == 0 )
                return false;

            std::copy( data.data.begin(), data.data.begin() + data.size, room.buffer );
            buffer.commit_transmit_buffer( room );

            return true;
        }

        /**
         * @brief hands over the oldest PDUs received in connection events and forgets them
         *
         * What a program's read_received() took out of the buffers comes first, then what is
         * still in them, the first buffer before the second.
         */
        received_batch collect_received()
        {
            received_batch batch;

            for ( ; batch.count != received_per_batch && read_head_ != read_count_; ++read_head_ )
                batch.pdus[ batch.count++ ] = read_[ read_head_ ];

            for ( pdu_buffer& buffer : pdu_buffers_ )
            {
                for ( auto next = buffer.next_received(); batch.count != received_per_batch && next.size != 0; next = buffer.next_received() )
                {
                    batch.pdus[ batch.count ] = pdu( std::span< const std::uint8_t >( next.buffer, next.size ) );
                    buffer.free_received();
                    ++batch.count;
                }
            }

            return batch;
        }
        /** @} */

        /**
         * @name Acceptance filter
         * @{
         */

        /**
         * @brief adds an address to the acceptance filter set
         *
         * True if it was added or was already there, false if the set is full. There is no
         * removal: the reset before every test empties the set, as it does the program.
         */
        bool add_to_acceptance_filter( const link_layer::device_address& address )
        {
            return acceptance_filter_.add( address );
        }
        /** @} */

        /**
         * @name Pairing toolbox
         *
         * The functions of lesc_pairing_toolbox that take a pointer, with the array the
         * pointer denotes as the parameter, since a serialiser cannot size a pointer. The
         * other four are the radio's own members, reached as toolbox_t. Only instantiated
         * if the radio has a toolbox, since only then does the list name them.
         * @{
         */

        /**
         * @brief the x coordinate of a public key, the u and v of f4() and g2()
         */
        using coordinate_t = std::array< std::uint8_t, 32 >;

        bluetoe::details::ecdh_shared_secret_t p256(
            const bluetoe::details::ecdh_private_key_t& private_key,
            const bluetoe::details::ecdh_public_key_t&  public_key )
        {
            return radio_t::p256( private_key.data(), public_key.data() );
        }

        bluetoe::details::uint128_t f4(
            const coordinate_t& u, const coordinate_t& v, const bluetoe::details::uint128_t& x, std::uint8_t z )
        {
            return radio_t::f4( u.data(), v.data(), x, z );
        }

        std::uint32_t g2(
            const coordinate_t& u, const coordinate_t& v,
            const bluetoe::details::uint128_t& x, const bluetoe::details::uint128_t& y )
        {
            return radio_t::g2( u.data(), v.data(), x, y );
        }
        /** @} */

        using functions = function_list<
            &dut_rig::protocol_version,
            &dut_rig::implementation_name,
            &dut_rig::build_identifier,
            &dut_rig::set_session_token,
            &dut_rig::program_finished,
            &dut_rig::properties,
            &dut_rig::add_step,
            &dut_rig::add_call,
            &dut_rig::start_program,
            &dut_rig::collect_records,
            &dut_rig::add_to_acceptance_filter,
            &dut_rig::queue_pdu,
            &dut_rig::collect_received,
            &toolbox_t::generate_keys,
            &toolbox_t::select_random_nonce,
            &wrapped_t::p256,
            &wrapped_t::f4,
            &toolbox_t::f5,
            &toolbox_t::f6,
            &wrapped_t::g2 >;

    private:
        /*
         * A call as the program keeps it: what the wire call carries, with the advertising PDUs
         * it can hold and the place of the data channel PDU of a queue_pdu, which lives in the
         * program's pool.
         */
        struct stored_call
        {
            call_kind                                       kind            = call_kind::cancel_radio_event;
            std::uint32_t                                   channel         = 0;
            link_layer::delta_time                          delay           = {};
            link_layer::delta_time                          end_delay       = {};
            adv_pdu                                         transmit        = {};
            adv_pdu                                         response        = {};
            link_layer::device_address                      address         = {};
            std::uint32_t                                   access_address  = 0;
            std::uint32_t                                   crc_init        = 0;
            link_layer::phy_ll_encoding::phy_ll_encoding_t  phy             = link_layer::phy_ll_encoding::le_1m_phy;
            std::uint8_t                                    pdu             = 0;
        };

        /*
         * The library's PDU buffer is written to be a base of the radio. The rig owns it
         * instead and hands it to the radio, so this makes the radio's side of it public and
         * provides what the buffer asks of its radio: the lock, and the CCM counters, which
         * stay unused without encryption.
         */
        class pdu_buffer : public link_layer::ll_data_pdu_buffer< pdu_buffer_size, pdu_buffer_size, pdu_buffer >
        {
        public:
            using base_t     = link_layer::ll_data_pdu_buffer< pdu_buffer_size, pdu_buffer_size, pdu_buffer >;

            // excludes the radio context, which uses the buffer
            using lock_guard = typename radio_t::radio_lock_guard;

            using base_t::allocate_receive_buffer;
            using base_t::received;
            using base_t::next_transmit;

            void increment_receive_packet_counter() {}
            void increment_transmit_packet_counter() {}
        };

        void on_callback( callback_kind kind, link_layer::abs_time when, const adv_pdu& data, link_layer::connection_event_events events = {} )
        {
            record entry;
            entry.kind      = record_kind::callback;
            entry.callback  = kind;
            entry.when      = when;
            entry.data      = data;
            entry.events    = events;
            records_.push( entry );

            run_step_if_waiting_for( kind, when );
        }

        void run_step_if_waiting_for( callback_kind kind, link_layer::abs_time when )
        {
            if ( !running_ || cursor_ == step_count_ || steps_[ cursor_ ].on != kind )
                return;

            const program_step& current = steps_[ cursor_ ];
            ++cursor_;

            for ( std::size_t i = current.first_call; i != current.first_call + current.call_count; ++i )
                execute( calls_[ i ], when );
        }

        /*
         * The PDUs stay where the program keeps them, since the radio uses them until the
         * event is over; the receive buffer is the rig's own.
         */
        void execute( const stored_call& what, link_layer::abs_time when )
        {
            const link_layer::write_buffer transmit{ what.transmit.data.data(), what.transmit.size };
            const link_layer::write_buffer response{ what.response.data.data(), what.response.size };
            const link_layer::read_buffer  receive{ receive_.data(), receive_.size() };

            record entry;
            entry.kind      = record_kind::call;
            entry.call      = what.kind;
            entry.channel   = what.channel;

            switch ( what.kind )
            {
            case call_kind::start_advertising:
                // it cannot refuse: the radio is idle whenever a step runs, and the time
                // is the radio's own to choose, so there is no result to record
                radio_t::start_advertising( what.channel, transmit, response, receive );
                radio_event_pending_ = true;
                break;
            case call_kind::schedule_advertising_event:
                entry.when   = when + what.delay;
                entry.result = radio_t::schedule_advertising_event( what.channel, entry.when, transmit, response, receive );
                radio_event_pending_ = radio_event_pending_ || entry.result;
                break;
            case call_kind::schedule_connection_event:
                entry.when   = when + what.delay;
                entry.result = radio_t::schedule_connection_event( what.channel, entry.when, when + what.end_delay );
                radio_event_pending_ = radio_event_pending_ || entry.result;
                break;
            case call_kind::schedule_timer:
                entry.when   = when + what.delay;
                entry.result = radio_t::schedule_timer( entry.when );
                timer_pending_ = timer_pending_ || entry.result;
                break;
            case call_kind::cancel_radio_event:
                entry.result = radio_t::cancel_radio_event();
                radio_event_pending_ = radio_event_pending_ && !entry.result;
                break;
            case call_kind::cancel_timer:
                entry.result = radio_t::cancel_timer();
                timer_pending_ = timer_pending_ && !entry.result;
                break;
            case call_kind::set_local_address:
                radio_t::set_local_address( what.address );
                break;
            case call_kind::set_access_address_and_crc_init:
                radio_t::set_access_address_and_crc_init( what.access_address, what.crc_init );
                break;
            case call_kind::set_phy:
                radio_t::set_phy( what.phy, what.phy );
                break;
            case call_kind::queue_pdu:
                entry.result = queue_pdu( program_pdus_[ what.pdu ] );
                break;
            case call_kind::read_received:
                read_received();
                break;
            case call_kind::switch_pdu_buffer:
                // between events: the radio takes the buffer anew for every event
                active_buffer_ = ( active_buffer_ + 1 ) % pdu_buffer_count;
                break;
            }

            records_.push( entry );
        }

        /*
         * Takes what the buffer received out of it, as a link layer would, so that it has room
         * again; the rig keeps the PDUs for the host.
         */
        void read_received()
        {
            pdu_buffer& buffer = link_layer_pdu_buffer();

            for ( auto next = buffer.next_received(); next.size != 0; next = buffer.next_received() )
            {
                if ( read_count_ != read_queue_size )
                    read_[ read_count_++ ] = pdu( std::span< const std::uint8_t >( next.buffer, next.size ) );

                buffer.free_received();
            }
        }

        /*
         * A step of the program: the callback it waits for, and its calls, a range of calls_,
         * which all steps share.
         */
        struct program_step
        {
            callback_kind   on;
            std::uint8_t    first_call;
            std::uint8_t    call_count;
        };

        std::array< program_step, max_steps >           steps_;
        std::uint8_t                                    step_count_             = 0;
        std::array< stored_call, max_calls >            calls_;
        std::uint8_t                                    call_count_             = 0;
        std::uint8_t                                    cursor_                 = 0;
        bool                                            running_                = false;
        bool                                            radio_event_pending_    = false;
        bool                                            timer_pending_          = false;

        std::array< pdu, max_queued_pdus >              program_pdus_;
        std::uint8_t                                    program_pdu_count_      = 0;

        std::array< std::uint8_t, max_advertising_pdu_size >        receive_;
        std::array< pdu_buffer, pdu_buffer_count >      pdu_buffers_;
        std::size_t                                     active_buffer_          = 0;

        std::array< pdu, read_queue_size >              read_;
        std::size_t                                     read_head_              = 0;
        std::size_t                                     read_count_             = 0;

        reported_queue< record, record_queue_size >     records_;

        address_set< max_acceptance_filter_entries >    acceptance_filter_;
    };
}
}

#endif
