#ifndef BLUETOE_UUID_HPP
#define BLUETOE_UUID_HPP

namespace bluetoe {
namespace details {

    template < std::uint64_t A, std::uint64_t B, std::uint64_t C, std::uint64_t D, std::uint64_t E >
    struct check_uuid_parameters
    {
        static_assert( A < 0x100000000,      "uuid: first group of bytes can not be longer than 4 bytes." );
        static_assert( B < 0x10000,          "uuid: second group of bytes can not be longer than 2 bytes." );
        static_assert( C < 0x10000,          "uuid: third group of bytes can not be longer than 2 bytes." );
        static_assert( D < 0x10000,          "uuid: 4th group of bytes can not be longer than 2 bytes." );
        static_assert( E < 0x1000000000000l, "uuid: last group of bytes can not be longer than 6 bytes." );
        typedef void type;
    };

    template < std::uint64_t UUID >
    struct check_uuid_parameter16
    {
        static_assert( UUID < 0x10000,       "uuid16: a 16 bit UUID can not be longer than 4 bytes." );
        typedef void type;
    };

    /**
     * @brief a 128-Bit UUID
     *
     * The class takes 5 parameters to store the UUID in the usual form like this:
     * @code{.cpp}
     * bluetoe::uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
     * @endcode
     */
    template <
        std::uint64_t A,
        std::uint64_t B,
        std::uint64_t C,
        std::uint64_t D,
        std::uint64_t E,
        typename = typename details::check_uuid_parameters< A, B, C, D, E >::type >
    struct uuid
    {
        static constexpr std::uint8_t bytes[ 16 ] = {
                ( E >> 0  ) & 0xff,
                ( E >> 8  ) & 0xff,
                ( E >> 16 ) & 0xff,
                ( E >> 24 ) & 0xff,
                ( E >> 32 ) & 0xff,
                ( E >> 40 ) & 0xff,
                ( D >> 0  ) & 0xff,
                ( D >> 8  ) & 0xff,
                ( C >> 0  ) & 0xff,
                ( C >> 8  ) & 0xff,
                ( B >> 0  ) & 0xff,
                ( B >> 8  ) & 0xff,
                ( A >> 0  ) & 0xff,
                ( A >> 8  ) & 0xff,
                ( A >> 16 ) & 0xff,
                ( A >> 24 ) & 0xff
            };
        static constexpr bool is_128bit = true;

        static std::uint16_t as_16bit() {
            return A & 0xffff;
        };
    };

    template <
        std::uint64_t A,
        std::uint64_t B,
        std::uint64_t C,
        std::uint64_t D,
        std::uint64_t E,
        typename F >
    constexpr std::uint8_t uuid< A, B, C, D, E, F >::bytes[ 16 ];

    /**
     * @brief a 16-Bit UUID
     */
    template < std::uint64_t UUID, typename = typename check_uuid_parameter16< UUID >::type >
    struct uuid16
    {
        static constexpr std::uint8_t bytes[ 2 ] = {
                ( UUID >> 0 ) & 0xff,
                ( UUID >> 8 ) & 0xff,
            };
        static constexpr bool is_128bit = false;

        static constexpr std::uint16_t as_16bit() {
            return UUID & 0xffff;
        };
    };

    template < std::uint64_t UUID, typename A >
    constexpr std::uint8_t uuid16< UUID, A >::bytes[ 2 ];

    /**
     * @brief the 128 bit Bluetooth Base UUID
     */
    struct bluetooth_base_uuid : uuid< 0x00000000 ,0x0000, 0x1000, 0x8000, 0x00805F9B34FB >
    {
        /**
         * @brief constructs a 128 bit uuid from a 16 bit Bluetooth UUID
         */
        template < std::uint64_t A >
        using from_16bit = uuid< A ,0x0000, 0x1000, 0x8000, 0x00805F9B34FB > ;
    };



}
}

#endif
