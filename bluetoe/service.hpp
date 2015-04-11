#ifndef BLUETOE_SERVICE_HPP
#define BLUETOE_SERVICE_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>

#include <cstddef>
#include <cassert>

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
    }

    template <
        std::uint32_t A,
        std::uint16_t B,
        std::uint16_t C,
        std::uint16_t D,
        std::uint64_t E,
        typename = typename details::check_service_uuid_parameters< A, B, C, D, E >::type >
    class service_uuid {
        attribute_access_result attribute_access( const attribute_access_arguments& );
    };

    template < typename ... Options >
    class service
    {
    public:
        // a service is a list of attributes
        static constexpr std::size_t number_of_attributes = 1;

        static attribute attribute_at( std::size_t );
    };

    template < typename ... Options >
    attribute service< Options... >::attribute_at( std::size_t index )
    {
        assert( index == 0 );
        return attribute{ bits( gatt_uuids::primary_service ), nullptr };
    }
}

#endif