#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_DUT_RIG_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_DUT_RIG_HPP

/**
 * @file dut_rig.hpp
 *
 * The platform independent half of a device under test: everything the contract in
 * tests/scheduled_radio/dut_rig.hpp asks for that is neither the radio nor the serial
 * port. See documentation/scheduled_radio_test_rig.md, decisions 16, 17 and 20.
 *
 * The rig is a template over the scheduled radio implementation and over the platform's
 * serial port; instrument/template_dut_rig.cpp shows how a platform binds the two. The
 * host instantiates the same template with dummies (host/dut_functions.hpp) to obtain the function
 * list the wire is keyed on, which is why no function of the list carries a type of the
 * radio.
 *
 * This is the first slice: the instrument functions, the session token and the main loop.
 * The toolbox wrappers, the program interpreter and the records follow.
 */

#include "instrument/dispatcher.hpp"
#include "link/frame.hpp"
#include "link/function_list.hpp"
#include "link/ring_buffer.hpp"
#include "link/serial_port.hpp"
#include "link/serialize.hpp"

#include <bluetoe/link_layer/scheduled_radio2.hpp>
#include <bluetoe/radio_properties.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief version of the wire protocol of the device under test
     *
     * Changes whenever dut_rig::functions changes.
     */
    constexpr std::uint16_t dut_protocol_version = 1;

    /**
     * @brief the rig around a scheduled radio implementation
     *
     * Radio is the implementation under test, a template over the type that receives its
     * callbacks; a platform binds the implementation's options with an alias template
     * and passes that. The rig is that type: it derives from the radio and passes itself,
     * as the link layer does, and it receives the callbacks in the link layer context of
     * decision 17. Port is the platform's serial port, a template over the buffer type
     * and the type it wakes, which is the radio. MaxPayload bounds a request and a
     * response, and thereby the buffers.
     *
     * The concepts are checked in the constructor rather than in the declaration, because
     * a class cannot name itself in its own constraint.
     *
     * The rig is constructed at startup, so that the session token reads as zero after
     * every restart.
     */
    template <
        template < typename CallBacks > class Radio,
        template < typename Buffer, typename Wake > class Port,
        std::size_t MaxPayload = 256 >
    class dut_rig : public Radio< dut_rig< Radio, Port, MaxPayload > >
    {
    public:
        /**
         * @brief bytes of a name on the wire; longer names are truncated
         */
        static constexpr std::size_t name_size = 32;

        dut_rig( std::string_view implementation_name, std::string_view build_identifier )
            : implementation_name_( name( implementation_name ) )
            , build_identifier_( name( build_identifier ) )
            , session_token_( 0 )
            , port_( receive_, transmit_, *this )
            , receiver_( receive_ )
            , sender_( transmit_ )
            , dispatcher_( *this )
            , response_()
        {
            static_assert( link_layer::scheduled_radio< Radio, dut_rig > );
            static_assert( serial_port< port_t, buffer_t, radio_t > );

            port_.start();
        }

        /**
         * @brief one iteration of the main loop: answer a buffered request, then let the radio run
         *
         * Answering never waits for the port. A response that does not fit into the
         * transmit buffer is kept and handed over on a later iteration, before the next
         * request is read; the host does not send one before it has the answer anyway
         * (decision 6). A corrupt frame is dropped without an answer, and the host times out.
         */
        void run()
        {
            if ( !pending_ && receiver_.receive() == receive_result::frame )
            {
                buffer_sink out( response_ );

                if ( !dispatcher_.dispatch( receiver_.payload(), session_token_, out ) )
                    assert( !"response_ is smaller than a response of the function list" );

                pending_ = out.size();
            }

            if ( pending_ && sender_.send( { response_.data(), *pending_ } ) )
            {
                pending_.reset();
                port_.transmit_pending();
            }

            radio_t::run();
        }

        /**
         * @name Callbacks of the radio
         *
         * Received and dropped until the program interpreter and the records arrive with
         * decision 11, step 3. The callbacks of connection events come with the connection
         * events.
         * @{
         */
        void radio_ready() {}
        void adv_received( link_layer::abs_time, const link_layer::read_buffer& ) {}
        void adv_timeout( link_layer::abs_time ) {}
        void user_timer( link_layer::abs_time ) {}
        /** @} */

        /**
         * @name Instrument functions
         *
         * The functions of instrument.hpp and dut_rig.hpp, in the order of the list.
         * @{
         */
        std::uint16_t protocol_version() const
        {
            return dut_protocol_version;
        }

        bytes< name_size > implementation_name() const
        {
            return implementation_name_;
        }

        bytes< name_size > build_identifier() const
        {
            return build_identifier_;
        }

        /**
         * @brief the token every response carries from now on
         *
         * The response to this call still carries the token that was in effect when the
         * request arrived: zero on a freshly started instrument, which is what proves a
         * reset to the host.
         */
        void set_session_token( std::uint32_t token )
        {
            session_token_ = token;
        }

        /**
         * @brief whether the loaded program ran to its end
         *
         * No program can be loaded yet, and instrument.hpp asks for false in that case.
         */
        bool program_finished() const
        {
            return false;
        }

        link_layer::radio_properties properties() const
        {
            return link_layer::radio_properties( static_cast< const radio_t& >( *this ) );
        }
        /** @} */

        using functions = function_list<
            &dut_rig::protocol_version,
            &dut_rig::implementation_name,
            &dut_rig::build_identifier,
            &dut_rig::set_session_token,
            &dut_rig::program_finished,
            &dut_rig::properties >;

    private:
        using radio_t  = Radio< dut_rig >;
        using buffer_t = ring_buffer< std::uint8_t, MaxPayload + frame_overhead >;
        using port_t   = Port< buffer_t, radio_t >;

        static bytes< name_size > name( std::string_view text )
        {
            bytes< name_size > result;

            result.size = std::min( text.size(), name_size );
            std::copy_n( text.begin(), result.size, result.data.begin() );

            return result;
        }

        bytes< name_size >                          implementation_name_;
        bytes< name_size >                          build_identifier_;
        std::uint32_t                               session_token_;

        buffer_t                                    receive_;
        buffer_t                                    transmit_;
        port_t                                      port_;
        frame_receiver< MaxPayload, buffer_t >      receiver_;
        frame_sender< buffer_t >                    sender_;
        dispatcher< functions, dut_rig >            dispatcher_;

        std::array< std::uint8_t, MaxPayload >      response_;
        std::optional< std::size_t >                pending_;
    };
}
}

#endif
