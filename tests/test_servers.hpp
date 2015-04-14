#ifndef BLUETOE_TESTS_TEST_SERVERS_HPP
#define BLUETOE_TESTS_TEST_SERVERS_HPP

#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

namespace {
    unsigned temperature_value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
                bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
                bluetoe::no_write_access
            >
        >
    > small_temperature_service;

    template < class Server, std::size_t ResponseBufferSize = 23 >
    struct request_with_reponse : Server
    {
        request_with_reponse()
            : response_size( ResponseBufferSize )
        {
            std::fill( std::begin( response ), std::end( response ), 0x55 );
        }

        template < std::size_t PDU_Size >
        void l2cap_input( const std::uint8_t(&input)[PDU_Size] )
        {
            Server::l2cap_input( input, PDU_Size, response, response_size );
        }

        static_assert( ResponseBufferSize >= 23, "MTU is 23, no point in using less" );

        std::uint8_t response[ ResponseBufferSize ];
        std::size_t  response_size;
    };

    template < std::size_t ResponseBufferSize = 23 >
    using small_temperature_service_with_response = request_with_reponse< small_temperature_service, ResponseBufferSize >;

}

#endif
