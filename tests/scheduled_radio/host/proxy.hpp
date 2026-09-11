#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_PROXY_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_PROXY_HPP

/**
 * @file proxy.hpp
 *
 * The host side of the request and response protocol of link/function_list.hpp: a call on
 * the proxy becomes a request, and the response becomes the result or an error.
 */

#include "host/errors.hpp"
#include "link/function_list.hpp"
#include "link/serialize.hpp"
#include "link/status.hpp"

#include <concepts>
#include <cstdint>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief sends a request payload and waits for the response payload
     *
     * Throws link_error if no response arrives or the response frame is corrupt.
     */
    template < typename T >
    concept transport = requires ( T t, std::span< const std::uint8_t > request )
    {
        { t.transact( request ) } -> std::same_as< std::vector< std::uint8_t > >;
    };

    /**
     * @brief a sink that grows
     */
    class vector_sink
    {
    public:
        explicit vector_sink( std::vector< std::uint8_t >& storage )
            : storage_( storage ) {}

        bool write( const std::uint8_t* data, std::size_t size )
        {
            storage_.insert( storage_.end(), data, data + size );

            return true;
        }

    private:
        std::vector< std::uint8_t >& storage_;
    };

    /**
     * @brief calls the functions of a list on the instrument at the other end of a transport
     *
     * Template parameters are the set of function to be called and the transport type.
     */
    template < typename List, transport Transport >
    class proxy;

    template < auto... Functions, transport Transport >
    class proxy< function_list< Functions... >, Transport >
    {
    public:
        using list = function_list< Functions... >;

        /**
         * @brief takes a reference to the transport that is used to serialize function calls
         */
        explicit proxy( Transport& transport )
            : transport_( transport )
            , expected_token_( 0 )
        {}

        /**
         * @brief the session token every response has to carry from now on
         *
         * The token was initialized to zero, so zero is the only value that
         * should not be used.
         */
        void expect_token( std::uint32_t token )
        {
            assert( token != 0 );
            expected_token_ = token;
        }

        /**
         * @brief call F on the instrument
         *
         * The arguments are converted to the parameter types of F before they are
         * serialised, so what goes on the wire is defined by F's signature alone.
         *
         * @throws link_error           the response is missing, malformed, or reports a status
         * @throws instrument_restarted the response carries the wrong session token
         */
        template < auto F, typename... Args >
        typename member_function_traits< decltype( F ) >::result call( Args&&... args )
        {
            using traits = member_function_traits< decltype( F ) >;

            constexpr std::size_t opcode = opcode_of< F, list >::value;
            static_assert( opcode < list::size, "the function is not in the list" );

            std::vector< std::uint8_t > request;
            vector_sink                 out( request );

            serialize( out, static_cast< std::uint8_t >( opcode ) );
            serialize( out, typename traits::arguments( std::forward< Args >( args )... ) );

            const std::vector< std::uint8_t > response = transport_.transact( request );

            buffer_source in( response );
            std::uint32_t token;
            status        result_status;

            if ( !deserialize( in, token ) || !deserialize( in, result_status ) )
                throw link_error( "response shorter than token and status" );

            if ( token != expected_token_ )
                throw instrument_restarted( expected_token_, token );

            if ( result_status != status::ok )
                throw link_error( describe( result_status ) );

            if constexpr ( std::is_void_v< typename traits::result > )
            {
                if ( in.remaining() != 0 )
                    throw link_error( "response of a void function carries a result" );
            }
            else
            {
                typename traits::result result{};

                if ( !deserialize( in, result ) )
                    throw link_error( "response shorter than the result" );

                if ( in.remaining() != 0 )
                    throw link_error( "response longer than the result" );

                return result;
            }
        }

    private:
        Transport&      transport_;
        std::uint32_t   expected_token_;
    };
}
}

#endif
