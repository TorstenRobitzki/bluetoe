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
    class service_uuid : details::uuid< A, B, C, D, E >
    {
    public:
        static details::attribute_access_result attribute_access( details::attribute_access_arguments& );

        typedef details::service_uuid_meta_type meta_type;
    };

    /**
     * @brief a service with zero or more characteristics
     */
    template < typename ... Options >
    class service
    {
    public:
        typedef typename details::find_all_by_meta_type< details::characteristic_meta_type, Options... >::type characteristics;

        static constexpr std::size_t number_of_service_attributes        = 1;
        static constexpr std::size_t number_of_characteristic_attributes = details::sum_up_attributes< characteristics >::value;

        /**
         * a service is a list of attributes
         */
        static constexpr std::size_t number_of_attributes =
              number_of_service_attributes
            + number_of_characteristic_attributes;

        /**
         *
         */
        static details::attribute attribute_at( std::size_t index );

        /**
         * assembles one data packet for a "Read by Group Type Response"
         */
        static std::uint8_t* read_primary_service_response( std::uint8_t* output, std::uint8_t* end, std::uint16_t& starting_index );

        typedef details::service_meta_type meta_type;
    };

    template <
        std::uint32_t A,
        std::uint16_t B,
        std::uint16_t C,
        std::uint16_t D,
        std::uint64_t E >
    // service_uuid implementation
    details::attribute_access_result service_uuid< A, B, C, D, E >::attribute_access( details::attribute_access_arguments& args )
    {
        if ( args.type == details::attribute_access_type::read )
        {
            args.buffer_size = std::min< std::size_t >( sizeof( details::uuid< A, B, C, D, E >::bytes ), args.buffer_size );

            std::copy( std::begin( details::uuid< A, B, C, D, E >::bytes ), std::begin( details::uuid< A, B, C, D, E >::bytes ) + args.buffer_size, args.buffer );

            return args.buffer_size == 16
                ? details::attribute_access_result::success
                : details::attribute_access_result::read_truncated;
        }

        return details::attribute_access_result::write_not_permitted;
    }

    // service implementation
    template < typename ... Options >
    details::attribute service< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );

        if ( index == 0 )
            return details::attribute{ bits( details::gatt_uuids::primary_service ), &details::find_by_meta_type< details::service_uuid_meta_type, Options... >::type::attribute_access };

        return details::attribute_at_list< characteristics >::attribute_at( index -1 );
    }

    template < typename ... Options >
    std::uint8_t* service< Options... >::read_primary_service_response( std::uint8_t* output, std::uint8_t* end, std::uint16_t& starting_index )
    {
        if ( end - output >= 20 )
        {
            std::uint8_t* const old_output = output;

            output = details::write_handle( output, starting_index );
            output = details::write_handle( output, starting_index + number_of_attributes -1 );

            const details::attribute primary_service = attribute_at( 0 );

            auto read = details::attribute_access_arguments::read( output, end );

            if ( primary_service.access( read ) == details::attribute_access_result::success )
            {
                output         += read.buffer_size;
                starting_index += number_of_attributes;
            }
            else
            {
                output = old_output;
            }
        }

        return output;
    }

}

#endif