#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_INSTRUMENT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_INSTRUMENT_HPP

/**
 * @file instrument.hpp
 *
 * What the two instruments have in common, the device under test and the tester: the
 * contract of tests/scheduled_radio/instrument.hpp, minus the programs and the queues,
 * which follow. The names an instrument reports, the session token, the two ring buffers
 * the platform's port works on, the framing, and the step that answers a request. See
 * documentation/scheduled_radio_test_rig.md, decisions 6, 8 and 16.
 *
 * An instrument derives from this class and passes itself: it is what the port wakes, and
 * its function list is what requests are dispatched against. The list names the functions
 * of this class through the derived class, &dut_rig::implementation_name, which is why the
 * list can be built by the derived class alone and the dispatcher is built here on the fly.
 */

#include "instrument/dispatcher.hpp"
#include "link/frame.hpp"
#include "link/ring_buffer.hpp"
#include "link/serial_port.hpp"
#include "link/serialize.hpp"

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
     * @brief the platform independent part every instrument consists of
     *
     * Derived is the instrument, which provides its function list as Derived::functions,
     * a wake_up() callable from the port's context, and a main loop that calls serve().
     * Port is the platform's serial port, a template over the buffer type and the type it
     * wakes. MaxPayload bounds a request and a response, and thereby the buffers.
     *
     * An instrument is constructed at startup, so that the session token reads as zero
     * after every restart.
     */
    template <
        typename Derived,
        template < typename Buffer, typename Wake > class Port,
        std::size_t MaxPayload >
    class instrument
    {
    public:
        /**
         * @brief bytes of a name on the wire; longer names are truncated
         */
        static constexpr std::size_t name_size = 32;

        /**
         * @name Instrument functions
         *
         * The functions of instrument.hpp that are the same on both instruments; the
         * derived class lists them in the order of its wire protocol.
         * @{
         */
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
        /** @} */

    protected:
        instrument( std::string_view implementation_name, std::string_view build_identifier )
            : implementation_name_( name( implementation_name ) )
            , build_identifier_( name( build_identifier ) )
            , session_token_( 0 )
            , port_( receive_, transmit_, derived() )
            , receiver_( receive_ )
            , sender_( transmit_ )
            , response_()
        {
        }

        /**
         * @brief starts the port
         *
         * Called by the derived class at the end of its constructor, since from here on
         * the port wakes it from interrupt context.
         */
        void start()
        {
            static_assert( serial_port< port_t, buffer_t, Derived > );

            port_.start();
        }

        /**
         * @brief answer a buffered request
         *
         * Never waits for the port. A response that does not fit into the transmit buffer
         * is kept and handed over on a later call, before the next request is read; the
         * host does not send one before it has the answer anyway (decision 6). A corrupt
         * frame is dropped without an answer, and the host times out.
         */
        void serve()
        {
            if ( !pending_ && receiver_.receive() == receive_result::frame )
            {
                buffer_sink                                             out( response_ );
                dispatcher< typename Derived::functions, Derived >      dispatch( derived() );

                if ( !dispatch.dispatch( receiver_.payload(), session_token_, out ) )
                    assert( !"response_ is smaller than a response of the function list" );

                pending_ = out.size();
            }

            if ( pending_ && sender_.send( { response_.data(), *pending_ } ) )
            {
                pending_.reset();
                port_.transmit_pending();
            }
        }

    private:
        using buffer_t = ring_buffer< std::uint8_t, MaxPayload + frame_overhead >;
        using port_t   = Port< buffer_t, Derived >;

        Derived& derived()
        {
            return static_cast< Derived& >( *this );
        }

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

        std::array< std::uint8_t, MaxPayload >      response_;
        std::optional< std::size_t >                pending_;
    };
}
}

#endif
