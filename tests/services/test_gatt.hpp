#ifndef BLUETOE_TESTS_SERVICE_TEST_DISCOVERY_HPP
#define BLUETOE_TESTS_SERVICE_TEST_DISCOVERY_HPP

#include "../test_servers.hpp"
#include "../hexdump.hpp"
#include <ostream>

namespace test {

    static constexpr std::uint16_t invalid_handle = 0;

    struct discovered_service
    {
        std::uint16_t starting_handle;
        std::uint16_t ending_handle;

        discovered_service( std::uint16_t start, std::uint16_t end );
        discovered_service();

        bool operator==( const discovered_service& rhs ) const;
    };

    std::ostream& operator<<( std::ostream& out, const discovered_service& );

    struct discovered_characteristic
    {
        std::uint16_t declaration_handle;
        std::uint16_t value_handle;

        discovered_characteristic( std::uint16_t decl, std::uint16_t value );
        discovered_characteristic();

        bool operator==( const discovered_characteristic& rhs ) const;
    };

    std::ostream& operator<<( std::ostream& out, const discovered_characteristic& );

    template < typename Server >
    struct gatt_procedures : request_with_reponse< Server >
    {
        static std::uint8_t low( std::uint16_t b )
        {
            return b & 0xff;
        }

        static std::uint8_t high( std::uint16_t b )
        {
            return b >> 8;
        }

        void add_handle( std::vector< std::uint8_t >& buffer, std::uint16_t handle )
        {
            buffer.push_back( low( handle ) );
            buffer.push_back( high( handle ) );
        }

        void add_handle( std::vector< std::uint8_t >& buffer, std::uint16_t h1, std::uint16_t h2 )
        {
            add_handle( buffer, h1 );
            add_handle( buffer, h2 );
        }

        void add_uuid( std::vector< std::uint8_t >& buffer, std::uint16_t uuid )
        {
            return add_handle( buffer, uuid );
        }

        std::uint16_t handle_at( std::size_t pos )
        {
            return this->response[ pos ] | ( this->response[ pos + 1 ] <<  8);
        }

        template < class UUID >
        discovered_service discover_primary_service_by_uuid()
        {
            std::vector< std::uint8_t > request = { 0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 };
            request.insert( request.end(), std::begin( UUID::bytes ), std::end( UUID::bytes ) );

            this->l2cap_input( request, this->connection );

            BOOST_REQUIRE_GT( this->response_size, 0 );

            if ( this->response[ 0 ] == 0x07 )
            {
                BOOST_REQUIRE_EQUAL( this->response_size, 5 );
                return discovered_service(
                    this->response[ 1 ] | ( this->response[ 2 ] << 8 ),
                    this->response[ 3 ] | ( this->response[ 4 ] << 8 ) );
            }

            return discovered_service();
        }

        template < class UUID >
        discovered_characteristic discover_characteristic_by_uuid( const discovered_service& service )
        {
            std::uint8_t last_response_code = 0x09;

            for ( std::uint16_t start_handle = service.starting_handle; last_response_code == 0x09;  )
            {
                std::vector< std::uint8_t > request = { 0x08 };
                add_handle( request, start_handle, service.ending_handle );
                add_uuid( request, 0x2803 );

                this->l2cap_input( request, this->connection );
                BOOST_REQUIRE_GT( this->response_size, 0 );

                last_response_code = this->response[ 0 ];
                if ( last_response_code == 0x09 )
                {
                    BOOST_REQUIRE_GT( this->response_size, 1 );
                    const std::size_t length = this->response[ 1 ];

                    BOOST_REQUIRE( length == 2 + 3 + sizeof( UUID::bytes ) );
                    BOOST_REQUIRE_EQUAL( ( this->response_size - 2 ) % length, 0 );
                    BOOST_REQUIRE_GT( ( this->response_size - 2 ) / length, 0 );

                    for ( std::size_t ptr = 2; ptr != this->response_size; ptr += length )
                    {
                        start_handle = handle_at( ptr + 3 ) + 1;
                        if ( std::equal( std::begin( UUID::bytes ), std::end( UUID::bytes ), &this->response[ ptr + 2 + 3 ]) )
                        {
                            return discovered_characteristic( handle_at( ptr ), handle_at( ptr + 3 ) );
                        }
                    }
                }
            }

            return discovered_characteristic();
        }
    };
}

#endif
