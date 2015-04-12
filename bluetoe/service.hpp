#ifndef BLUETOE_SERVICE_HPP
#define BLUETOE_SERVICE_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/options.hpp>

#include <cstddef>
#include <cassert>
#include <algorithm>

namespace bluetoe {

    template < const char* const >
    class service_name {};

    namespace details {
        template < std::uint32_t A, std::uint16_t B, std::uint16_t C, std::uint16_t D, std::uint64_t E >
        struct check_service_uuid_parameters
        {
            static_assert( E < 0x1000000000000l, "service_uuid: last group of bytes can not be larger than 6 bytes." );
            typedef void type;
        };

        struct service_uuid_meta_type {};
        struct service_meta_type {};
    }

    /**
     * @brief a 128-Bit UUID used to identify a service.
     *
     * The class takes 5 parameters to store the UUID in the usual form like this:
     * bluetoe::service_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
     */
    template <
        std::uint32_t A,
        std::uint16_t B,
        std::uint16_t C,
        std::uint16_t D,
        std::uint64_t E,
        typename = typename details::check_service_uuid_parameters< A, B, C, D, E >::type >
    class service_uuid
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
        // a service is a list of attributes
        static constexpr std::size_t number_of_attributes = 1;

        static details::attribute attribute_at( std::size_t );

        typedef details::service_meta_type meta_type;
    };

    template <
        std::uint32_t A,
        std::uint16_t B,
        std::uint16_t C,
        std::uint16_t D,
        std::uint64_t E,
        typename F >
    // service_uuid implementation
    details::attribute_access_result service_uuid< A, B, C, D, E, F >::attribute_access( details::attribute_access_arguments& args )
    {
        static constexpr uint8_t uuid[ 16 ] = {
            ( A >> 24 ) & 0xff,
            ( A >> 16 ) & 0xff,
            ( A >> 8  ) & 0xff,
            ( A >> 0  ) & 0xff,
            ( B >> 8  ) & 0xff,
            ( B >> 0  ) & 0xff,
            ( C >> 8  ) & 0xff,
            ( C >> 0  ) & 0xff,
            ( D >> 8  ) & 0xff,
            ( D >> 0  ) & 0xff,
            ( E >> 40 ) & 0xff,
            ( E >> 32 ) & 0xff,
            ( E >> 24 ) & 0xff,
            ( E >> 16 ) & 0xff,
            ( E >> 8  ) & 0xff,
            ( E >> 0  ) & 0xff
        };

        if ( args.type == details::attribute_access_type::read )
        {
            args.buffer_size = std::min< std::size_t >( 16, args.buffer_size );

            std::copy( std::begin( uuid ), std::begin( uuid ) + args.buffer_size, args.buffer );

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
        assert( index == 0 );
        return details::attribute{ bits( gatt_uuids::primary_service ), &details::find_by_meta_type< details::service_uuid_meta_type, Options... >::type::attribute_access };
    }
}

#endif