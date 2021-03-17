#ifndef BLUETOE_TESTS_SERVICE_TEST_DISCOVERY_HPP
#define BLUETOE_TESTS_SERVICE_TEST_DISCOVERY_HPP

#include <boost/test/unit_test.hpp>

#include "test_servers.hpp"
#include "hexdump.hpp"
#include "test_uuid.hpp"

#include <ostream>
#include <vector>

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
        std::uint16_t end_handle;
        std::uint16_t value_handle;
        std::uint8_t  properties;
        dynamic_uuid  uuid;

        discovered_characteristic( std::uint16_t decl, std::uint16_t value, std::uint8_t  properties, const dynamic_uuid& uuid );
        discovered_characteristic();

        bool operator==( const discovered_characteristic& rhs ) const;
    };

    std::ostream& operator<<( std::ostream& out, const discovered_characteristic& );

    struct discovered_characteristic_descriptor
    {
        std::uint16_t   handle;
        std::uint16_t   uuid;

        discovered_characteristic_descriptor( std::uint16_t handle, std::uint16_t uuid );
        discovered_characteristic_descriptor();

        bool operator==( const discovered_characteristic_descriptor& rhs ) const;
    };

    std::ostream& operator<<( std::ostream& out, const discovered_characteristic_descriptor& );

    template < typename Server, std::size_t MTU = 23 >
    struct gatt_procedures : request_with_reponse< Server, MTU >
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

            BOOST_REQUIRE_GT( this->response_size, 0u );

            if ( this->response[ 0 ] == 0x07 )
            {
                BOOST_REQUIRE_EQUAL( this->response_size, 5u );
                return discovered_service(
                    this->response[ 1 ] | ( this->response[ 2 ] << 8 ),
                    this->response[ 3 ] | ( this->response[ 4 ] << 8 ) );
            }

            return discovered_service();
        }

        std::vector< discovered_characteristic > discover_all_characteristics_of_a_service( const discovered_service& service )
        {
            std::vector< discovered_characteristic > result;

            std::uint8_t last_response_code = 0x09;

            for ( std::uint16_t start_handle = service.starting_handle; last_response_code == 0x09;  )
            {
                std::vector< std::uint8_t > request = { 0x08 };
                add_handle( request, start_handle, service.ending_handle );
                add_uuid( request, 0x2803 );

                this->l2cap_input( request, this->connection );
                BOOST_REQUIRE_GT( this->response_size, 0u );

                last_response_code = this->response[ 0 ];
                if ( last_response_code == 0x09 )
                {
                    BOOST_REQUIRE_GT( this->response_size, 1u );
                    const std::size_t length = this->response[ 1 ];

                    BOOST_REQUIRE( length == 2 + 3 + 2 || length == 2 + 3 + 16 );
                    BOOST_REQUIRE_EQUAL( ( this->response_size - 2 ) % length, 0u );
                    BOOST_REQUIRE_GT( ( this->response_size - 2 ) / length, 0u );

                    for ( std::size_t ptr = 2; ptr != this->response_size; ptr += length )
                    {
                        start_handle = handle_at( ptr + 3 ) + 1;
                        dynamic_uuid uuid( &this->response[ ptr + 5 ], length - 5 );

                        result.push_back(
                            discovered_characteristic( handle_at( ptr ), handle_at( ptr + 3 ), this->response[ ptr + 2 ], uuid ) );
                    }
                }
            }

            for ( std::vector< discovered_characteristic >::iterator uuid = result.begin(); uuid != result.end(); ++uuid )
            {
                const auto next = uuid +1;
                uuid->end_handle = next != result.end()
                    ? next->declaration_handle - 1
                    : service.ending_handle;
            }

            return result;
        }

        template < class UUID >
        discovered_characteristic discover_characteristic_by_uuid( const discovered_service& service )
        {
            const std::vector< discovered_characteristic > all = discover_all_characteristics_of_a_service( service );
            const dynamic_uuid service_uuid( static_cast< const UUID* >( nullptr ) );

            auto result = std::find_if( all.begin(), all.end(),
                [ &service_uuid ]( const discovered_characteristic& c ){ return c.uuid == service_uuid ; } );

            return result != all.end()
                ? *result
                : discovered_characteristic();
        }

        std::vector< discovered_characteristic_descriptor > discover_all_characteristic_descriptors( const discovered_characteristic& characteristic )
        {
            std::vector< discovered_characteristic_descriptor > result;

            std::uint8_t last_response_code = 0x05;

            for ( std::uint16_t start_handle = characteristic.value_handle +1; last_response_code == 0x05;  )
            {
                std::vector< std::uint8_t > request = { 0x04 };
                add_handle( request, start_handle, characteristic.end_handle );

                this->l2cap_input( request, this->connection );
                BOOST_REQUIRE_GT( this->response_size, 0u );

                last_response_code = this->response[ 0 ];
                if ( last_response_code == 0x05 )
                {
                    BOOST_REQUIRE_GT( this->response_size, 1u );
                    const std::size_t format = this->response[ 1 ];

                    // 16 bit uuid
                    BOOST_REQUIRE( format == 0x01 );
                    static constexpr std::size_t length = 4;
                    BOOST_REQUIRE_EQUAL( ( this->response_size - 2 ) % length, 0u );
                    BOOST_REQUIRE_GT( ( this->response_size - 2 ) / length, 0u );

                    for ( std::size_t ptr = 2; ptr != this->response_size; ptr += length )
                    {
                        start_handle = handle_at( ptr ) + 1;

                        result.push_back(
                            discovered_characteristic_descriptor( handle_at( ptr ), handle_at( ptr + 2 ) ) );
                    }
                }
            }

            return result;
        }

        discovered_characteristic_descriptor discover_cccd( const discovered_characteristic& characteristic )
        {
            const std::vector< discovered_characteristic_descriptor > descriptors = discover_all_characteristic_descriptors( characteristic );

            const auto result = std::find_if( descriptors.begin(), descriptors.end(),
                []( const discovered_characteristic_descriptor& d ){ return d.uuid == 0x2902; } );

            return result != descriptors.end()
                ? *result
                : discovered_characteristic_descriptor();
        }
    };
}

#endif
