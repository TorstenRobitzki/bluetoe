#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_DISPATCHER_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_DISPATCHER_HPP

/**
 * @file dispatcher.hpp
 *
 * The instrument side of the request and response protocol of link/function_list.hpp.
 *
 * The dispatcher never blocks (unless the called function blocks) and never throws.
 * It answers an unknown opcode or arguments that do not deserialise, or that leave
 * bytes over, with a status instead of a result, and the host turns that into a
 * link error.
 *
 * A function of the list whose class is neither one of the dispatcher's objects nor a
 * base of one is not implemented on this instrument, and its opcode is answered with
 * status::unsupported_function. This is how the list stays the same on every instrument
 * while a feature is absent on some.
 */

#include "link/function_list.hpp"
#include "link/serialize.hpp"
#include "link/status.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace bluetoe {
namespace test_rig {

    namespace details {

        /*
         * The first of Objects that is Object or derives from it. A function inherited
         * from a base class is a member of that base, and the object it is called on is
         * the derived one.
         */
        template < typename Object, typename... Objects >
        constexpr std::size_t object_index()
        {
            constexpr bool matches[] = { std::is_base_of_v< Object, Objects >..., false };

            for ( std::size_t i = 0; i != sizeof...( Objects ); ++i )
                if ( matches[ i ] )
                    return i;

            return sizeof...( Objects );
        }
    }

    /**
     * @brief executes requests against the objects the functions of the list belong to
     *
     * A class a function of the list belongs to is, or is a base of, at most one of
     * Objects; a function without an object is unsupported on this instrument.
     */
    template < typename List, typename... Objects >
    class dispatcher;

    template < auto... Functions, typename... Objects >
    class dispatcher< function_list< Functions... >, Objects... >
    {
    public:
        static_assert( sizeof...( Functions ) > 0, "a dispatcher needs at least one function" );
        static_assert( sizeof...( Functions ) <= 256, "an opcode is one byte" );

        /*
         * Pointers rather than references, so that an object may hold its dispatcher as a
         * member: a tuple of references to a still incomplete class trips the tuple's
         * assignability checks, a tuple of pointers does not.
         */
        explicit dispatcher( Objects&... objects )
            : objects_( &objects... ) {}

        /**
         * @brief execute one request and write the response
         *
         * @param token the session token the response carries
         *
         * @return false only if the response does not fit into `response`, which is a
         *         sizing error of the caller; the payload of a request is bounded by the
         *         frame and the result of every function is bounded by its type.
         */
        template < sink Sink >
        bool dispatch( std::span< const std::uint8_t > request, std::uint32_t token, Sink& response )
        {
            if ( !serialize( response, token ) )
                return false;

            buffer_source in( request );
            std::uint8_t  opcode;

            if ( !deserialize( in, opcode ) )
                return serialize( response, status::malformed_arguments );

            if ( opcode >= sizeof...( Functions ) )
                return serialize( response, status::unknown_function );

            using handler = bool ( dispatcher::* )( buffer_source&, Sink& );
            static constexpr handler handlers[] = { &dispatcher::template invoke< Functions, Sink >... };

            return ( this->*handlers[ opcode ] )( in, response );
        }

    private:
        template < auto F, sink Sink >
        bool invoke( buffer_source& in, Sink& response )
        {
            using traits = member_function_traits< decltype( F ) >;

            constexpr std::size_t index = details::object_index< typename traits::object, Objects... >();

            if constexpr ( index == sizeof...( Objects ) )
            {
                return serialize( response, status::unsupported_function );
            }
            else
            {
                typename traits::arguments arguments;

                if ( !deserialize( in, arguments ) || in.remaining() != 0 )
                    return serialize( response, status::malformed_arguments );

                typename traits::object& object = *std::get< index >( objects_ );

                if constexpr ( std::is_void_v< typename traits::result > )
                {
                    std::apply( [ & ]( const auto&... args ) { ( object.*F )( args... ); }, arguments );

                    return serialize( response, status::ok );
                }
                else
                {
                    const auto result = std::apply( [ & ]( const auto&... args ) { return ( object.*F )( args... ); }, arguments );

                    return serialize( response, status::ok ) && serialize( response, result );
                }
            }
        }

        std::tuple< Objects*... > objects_;
    };
}
}

#endif
