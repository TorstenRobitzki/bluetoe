#ifndef BLUETOE_CLIENT_CHARACTERISTIC_CONFIGURATION_HPP
#define BLUETOE_CLIENT_CHARACTERISTIC_CONFIGURATION_HPP

#include <bluetoe/service.hpp>

namespace bluetoe {
namespace details {


    // basically this has to find out, how much characteristics have a "Client Characteristic Configuration" and to provide 2 bits per characteristic
    template < typename ... Options >
    struct client_characteristic_configuration
    {
        client_characteristic_configuration() : v_( 0 ) {}

        typedef typename details::find_all_by_meta_type< details::service_meta_type, Options... >::type services;

        static constexpr std::size_t bits_per_characteristic                       = 2;
        static constexpr std::size_t number_of_client_characteristic_configuration = 2;

        std::uint16_t configuration( const void* characteristic_value ) const
        {
            return v_;
        };

        void configuration( const void* characteristic_value, std::uint16_t value )
        {
            v_ = value;
        }

        std::uint16_t v_;
    };

}
}

#endif