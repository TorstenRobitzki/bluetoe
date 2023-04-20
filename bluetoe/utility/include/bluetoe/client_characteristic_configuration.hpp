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
        {
        }

        constexpr explicit client_characteristic_configuration( std::uint8_t* data, std::size_t )
            : data_( data )
        {
        }

        std::uint16_t flags( std::size_t index ) const
        {
            assert( data_ );

            return ( data_[ index / 4 ] >> shift( index ) ) & 0x3;
        }

        void flags( std::size_t index, std::uint16_t new_flags )
        {
            assert( data_ );

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

        /**
         * @brief begin of the serialized CCCDs
         *
         * This can be used to store the state of the CCCDs in bonding data.
         */
        const std::uint8_t* serialized_cccds_begin() const
        {
            return std::begin( configs_ );
        }

        /**
         * @brief end of the serialized CCCDs
         *
         * This can be used to store the state of the CCCDs in bonding data.
         */
        const std::uint8_t* serialized_cccds_end() const
        {
            return std::end( configs_ );
        }

        /**
         * @brief begin of the serialized CCCDs
         *
         * This can be used to restores the state of the CCCDs from bonding data.
         */
        std::uint8_t* serialized_cccds_begin()
        {
            return std::begin( configs_ );
        }

        /**
         * @brief end of the serialized CCCDs
         *
         * This can be used to restores the state of the CCCDs from bonding data.
         */
        std::uint8_t* serialized_cccds_end()
        {
            return std::end( configs_ );
        }

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
