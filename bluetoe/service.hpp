#ifndef BLUETOE_SERVICE_HPP
#define BLUETOE_SERVICE_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/uuid.hpp>
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
        /**
         * a service is a list of attributes
         */
        static constexpr std::size_t number_of_attributes = 1;

        /**
         *
         */
        static details::attribute attribute_at( std::size_t index );

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
    }
}

#endif