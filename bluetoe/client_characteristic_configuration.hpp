#ifndef BLUETOE_CLIENT_CHARACTERISTIC_CONFIGURATION_HPP
#define BLUETOE_CLIENT_CHARACTERISTIC_CONFIGURATION_HPP

#include <cassert>
#include <cstdint>
#include <algorithm>

namespace bluetoe {
namespace details {

    /**
     * @brief somehow stronger typed pointer to the beginning of the array where client configurations are stored.
     *
     * In opposite to client_characteristic_configurations<>, this class is not a template.
     */
    class client_characteristic_configuration
    {
    public:
        constexpr client_characteristic_configuration()
            : data_( nullptr )
            , size_( 0 )
        {
        }

        constexpr explicit client_characteristic_configuration( std::uint8_t* data, std::size_t s )
            : data_( data )
            , size_( s )
        {
        }

        std::uint16_t flags( std::size_t index ) const
        {
            assert( data_ );
            assert( index < size_ );

            return ( data_[ index / 4 ] >> shift( index ) ) & 0x3;
        }

        void flags( std::size_t index, std::uint16_t new_flags )
        {
            assert( data_ );
            assert( index < size_ );

            // do not assert on new_flags as they might come from a client
            data_[ index / 4 ] = ( data_[ index / 4 ] & ~mask( index ) ) | ( ( new_flags & 0x03 ) << shift( index ) );
        }

        static constexpr std::size_t bits_per_config = 2;

    private:
        static constexpr std::size_t shift( std::size_t index )
        {
            return ( index % 4 ) * bits_per_config;
        }

        static constexpr std::uint8_t mask( std::size_t index )
        {
            return 0x03 << shift( index );
        }

        std::uint8_t*   data_;

        // this member is purly for debugging and can be removed
        std::size_t     size_;
    };

    /**
     * Store for configuration and state informations for characteristics that need such informations to be
     * stored among the connection. Such characteristics are characteristics with notification or indication
     * beeing enabled.
     */
    template < std::size_t Size >
    class client_characteristic_configurations
    {
    public:
        /**
         * This information is intendet to be used by l2cap/link layer implementations that want to
         * add informations for characteristics with notification/indication capabilities.
         */
        static constexpr std::size_t number_of_characteristics_with_configuration = Size;

        client_characteristic_configurations()
        {
            std::fill( std::begin( configs_ ), std::end( configs_ ), 0 );
        }

        client_characteristic_configuration client_configurations()
        {
            return client_characteristic_configuration( &configs_[ 0 ], Size );
        };

    private:
        std::uint8_t configs_[ ( Size * client_characteristic_configuration::bits_per_config + 7 ) / 8 ];
    };

    template <>
    class client_characteristic_configurations< 0 >
    {
    public:
        client_characteristic_configuration client_configurations()
        {
            return client_characteristic_configuration();
        }
    };

}
}

#endif
