#ifndef BLUETOE_SERVICE_HPP
#define BLUETOE_SERVICE_HPP

#include <cstdint>

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
    }

    template <
        std::uint32_t A,
        std::uint16_t B,
        std::uint16_t C,
        std::uint16_t D,
        std::uint64_t E,
        typename = typename details::check_service_uuid_parameters< A, B, C, D, E >::type >
    class service_uuid {
    };

    template < typename ... Options >
    class service {};
}

#endif