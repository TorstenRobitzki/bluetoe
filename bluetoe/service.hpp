#ifndef BLUETOE_SERVICE_HPP
#define BLUETOE_SERVICE_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/uuid.hpp>
#include <bluetoe/characteristic.hpp>
#include <bluetoe/bits.hpp>
#include <cstddef>
#include <cassert>
#include <algorithm>

namespace bluetoe {

    template < const char* const >
    class service_name {};

    namespace details {
        struct service_uuid_meta_type {};
        struct service_meta_type {};

        template < class UUID >
        struct attribute_access_impl
        {
            typedef UUID uuid;
            static details::attribute_access_result attribute_access( details::attribute_access_arguments&, std::uint16_t attribute_handle );
        };

    }

    /**
     * @brief a 128-Bit UUID used to identify a service.
     *
     * The class takes 5 parameters to store the UUID in the usual form like this:
     * @code{.cpp}
     * bluetoe::service_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
     * @endcode
     */
    template <
        std::uint32_t A,
        std::uint16_t B,
        std::uint16_t C,
        std::uint16_t D,
        std::uint64_t E >
    class service_uuid : public details::uuid< A, B, C, D, E >, public details::attribute_access_impl< details::uuid< A, B, C, D, E > >
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        typedef details::service_uuid_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief a 16-Bit UUID used to identify a service
     *
     * @code{.cpp}
     * bluetoe::service_uuid16< 0x1816 >
     * @endcode
     */
    template < std::uint64_t UUID >
    class service_uuid16 : public details::uuid16< UUID >, public details::attribute_access_impl< details::uuid16< UUID > >
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        typedef details::service_uuid_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief a service with zero or more characteristics
     */
    template < typename ... Options >
    class service
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        typedef typename details::find_all_by_meta_type< details::characteristic_meta_type, Options... >::type characteristics;
        typedef typename details::find_by_meta_type< details::service_uuid_meta_type, Options... >::type       uuid;

        static constexpr std::size_t number_of_service_attributes        = 1;
        static constexpr std::size_t number_of_characteristic_attributes = details::sum_by< characteristics, details::sum_by_attributes >::value;
        static constexpr std::size_t number_of_client_configs            = details::sum_by< characteristics, details::sum_by_client_configs >::value;

        /**
         * a service is a list of attributes
         */
        static constexpr std::size_t number_of_attributes =
              number_of_service_attributes
            + number_of_characteristic_attributes;

        typedef details::service_meta_type meta_type;

        /**
         * ClientCharacteristicIndex is the number of characteristics with a Client Characteristic Configuration attribute
         */
        template < std::size_t ClientCharacteristicIndex >
        static details::attribute attribute_at( std::size_t index );

        /**
         * returns the Characteristic Declaration attribute
         */
        static details::attribute characteristic_declaration_attribute();

        /**
         * @brief assembles one data packet for a "Read by Group Type Response"
         */
        static std::uint8_t* read_primary_service_response( std::uint8_t* output, std::uint8_t* end, std::uint16_t starting_index, bool is_128bit_filter );
        /** @endcond */
    };

    // service_uuid implementation
    /** @cond HIDDEN_SYMBOLS */
    template < class UUID >
    details::attribute_access_result details::attribute_access_impl< UUID >::attribute_access( details::attribute_access_arguments& args, std::uint16_t attribute_handle )
    {
        if ( args.type == details::attribute_access_type::read )
        {
            if ( args.buffer_offset > sizeof( uuid::bytes ) )
                return details::attribute_access_result::invalid_offset;

            args.buffer_size = std::min< std::size_t >( sizeof( uuid::bytes ) - args.buffer_offset, args.buffer_size );

            std::copy( std::begin( uuid::bytes ) + args.buffer_offset , std::begin( uuid::bytes ) + args.buffer_offset + args.buffer_size, args.buffer );

            return args.buffer_size == sizeof( uuid::bytes ) - args.buffer_offset
                ? details::attribute_access_result::success
                : details::attribute_access_result::read_truncated;
        }
        else if ( args.type == details::attribute_access_type::compare_value )
        {
            if ( sizeof( uuid::bytes ) == args.buffer_size
              && std::equal( std::begin( uuid::bytes ), std::end( uuid::bytes ), &args.buffer[ 0 ] ) )
            {
                return details::attribute_access_result::value_equal;
            }
        }

        return details::attribute_access_result::write_not_permitted;
    }

    // service implementation
    template < typename ... Options >
    template < std::size_t ClientCharacteristicIndex >
    details::attribute service< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );

        if ( index == 0 )
            return characteristic_declaration_attribute();

        return details::attribute_at_list< characteristics, ClientCharacteristicIndex >::attribute_at( index -1 );
    }

    template < typename ... Options >
    details::attribute service< Options... >::characteristic_declaration_attribute()
    {
        return details::attribute{ bits( details::gatt_uuids::primary_service ), &uuid::attribute_access };
    }

    template < typename ... Options >
    std::uint8_t* service< Options... >::read_primary_service_response( std::uint8_t* output, std::uint8_t* end, std::uint16_t starting_index, bool is_128bit_filter )
    {
        const std::size_t attribute_data_size = is_128bit_filter ? 16 + 4 : 2 + 4;

        if ( is_128bit_filter == uuid::is_128bit && end - output >= attribute_data_size )
        {
            std::uint8_t* const old_output = output;

            output = details::write_handle( output, starting_index );
            output = details::write_handle( output, starting_index + number_of_attributes -1 );

            // TODO: by using 0, we generate attribute_at two times with different parameters: might cause code bloat
            const details::attribute primary_service = attribute_at< 0 >( 0 );

            auto read = details::attribute_access_arguments::read( output, end, 0 );

            if ( primary_service.access( read, 1 ) == details::attribute_access_result::success )
            {
                output += read.buffer_size;
            }
            else
            {
                output = old_output;
            }
        }

        return output;
    }

    /** @endcond */

}

#endif