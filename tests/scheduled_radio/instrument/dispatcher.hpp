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

    /**
     * @brief executes requests against the objects the functions of the list belong to
     *
     * Every class a function of the list belongs to appears exactly once among Objects.
     */
    template < typename List, typename... Objects >
    class dispatcher;

    template < auto... Functions, typename... Objects >
    class dispatcher< function_list< Functions... >, Objects... >
    {
    public:
        static_assert( sizeof...( Functions ) > 0, "a dispatcher needs at least one function" );
        static_assert( sizeof...( Functions ) <= 256, "an opcode is one byte" );

        explicit dispatcher( Objects&... objects )
            : objects_( objects... ) {}

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

            typename traits::arguments arguments;

            if ( !deserialize( in, arguments ) || in.remaining() != 0 )
                return serialize( response, status::malformed_arguments );

            typename traits::object& object = std::get< typename traits::object& >( objects_ );

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

        std::tuple< Objects&... > objects_;
    };
}
}

#endif
