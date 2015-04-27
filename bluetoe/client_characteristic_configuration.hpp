#ifndef BLUETOE_CLIENT_CHARACTERISTIC_CONFIGURATION_HPP
#define BLUETOE_CLIENT_CHARACTERISTIC_CONFIGURATION_HPP

#include <bluetoe/service.hpp>

namespace bluetoe {
namespace details {

    class client_characteristic_configuration
    {
    public:
        client_characteristic_configuration() : data_( 0 )
        {
        }

        std::uint16_t flags() const
        {
            return data_;
        }

        void flags( std::uint16_t new_flags )
        {
            data_ = new_flags;
        }
    private:
        std::uint16_t   data_;
    };

    template < std::size_t Size >
    class client_characteristic_configurations
    {
    public:
        client_characteristic_configuration* client_configurations()
        {
            return &configs_[ 0 ];
        };

    private:
        client_characteristic_configuration configs_[ Size ];
    };

    template <>
    class client_characteristic_configurations< 0 >
    {
    public:
        client_characteristic_configuration* client_configurations()
        {
            return nullptr;
        }
    };

}
}

#endif