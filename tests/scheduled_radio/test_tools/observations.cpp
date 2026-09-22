#include "test_tools/observations.hpp"

#include "host/tester_time.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <numeric>

namespace bluetoe {
namespace test_rig {

    std::vector< std::uint8_t > advertising(
        std::size_t payload_size, std::uint8_t fill, std::uint8_t type, const link_layer::device_address& advertiser )
    {
        constexpr std::uint8_t tx_add = 0x40;

        std::vector< std::uint8_t > result( 2 + payload_size, fill );
        result[ 0 ] = type | ( advertiser.is_random() ? tx_add : 0 );
        result[ 1 ] = static_cast< std::uint8_t >( payload_size );
        std::copy( advertiser.begin(), advertiser.end(), result.begin() + 2 );

        return result;
    }

    std::vector< std::uint8_t > payload_of( std::size_t size, std::uint8_t first )
    {
        std::vector< std::uint8_t > result( size );
        std::iota( result.begin(), result.end(), first );

        return result;
    }

    std::array< std::uint8_t, 14 > scan_request(
        const link_layer::device_address& scanner, const link_layer::device_address& advertiser )
    {
        constexpr std::uint8_t scan_req  = 0x03;
        constexpr std::uint8_t tx_add    = 0x40;
        constexpr std::uint8_t rx_add    = 0x80;

        std::array< std::uint8_t, 14 > result = {};

        result[ 0 ] = scan_req
            | ( scanner.is_random() ? tx_add : 0 )
            | ( advertiser.is_random() ? rx_add : 0 );
        result[ 1 ] = 12;

        std::copy( scanner.begin(), scanner.end(), result.begin() + 2 );
        std::copy( advertiser.begin(), advertiser.end(), result.begin() + 8 );

        return result;
    }

    std::array< std::uint8_t, 36 > connect_request(
        const link_layer::device_address& initiator, const link_layer::device_address& advertiser )
    {
        constexpr std::uint8_t connect_ind = 0x05;
        constexpr std::uint8_t tx_add      = 0x40;
        constexpr std::uint8_t rx_add      = 0x80;

        std::array< std::uint8_t, 36 > result = {
            0, 34,
            0, 0, 0, 0, 0, 0,                   // InitA
            0, 0, 0, 0, 0, 0,                   // AdvA
            0x5a, 0xb3, 0x9a, 0xaf,             // access address
            0x08, 0x81, 0xf6,                   // CRC init
            0x03,                               // transmit window size: 3.75 ms
            0x0b, 0x00,                         // transmit window offset: 13.75 ms
            0x18, 0x00,                         // interval: 30 ms
            0x00, 0x00,                         // latency
            0x48, 0x00,                         // timeout: 720 ms
            0xff, 0xff, 0xff, 0xff, 0x1f,       // channel map
            0x25                                // hop 5, sleep clock accuracy 51-150 ppm
        };

        result[ 0 ] = connect_ind
            | ( initiator.is_random() ? tx_add : 0 )
            | ( advertiser.is_random() ? rx_add : 0 );

        std::copy( initiator.begin(), initiator.end(), result.begin() + 2 );
        std::copy( advertiser.begin(), advertiser.end(), result.begin() + 8 );

        return result;
    }

    std::chrono::microseconds time_between( const captured_pdu& earlier, const captured_pdu& later )
    {
        return std::chrono::duration_cast< std::chrono::microseconds >(
            time_of( later.when ) - time_of( earlier.when ) );
    }

    tester_duration inter_frame_space( const captured_pdu& earlier, const captured_pdu& later,
        link_layer::phy_ll_encoding::phy_ll_encoding_t phy )
    {
        const bool two_mbit = phy == link_layer::phy_ll_encoding::le_2m_phy;

        // preamble, access address, header, payload and CRC; the preamble is one byte at
        // 1 Mbit and two at 2 Mbit, and a byte takes 8 µs there and 4 µs at 2 Mbit
        const std::chrono::microseconds air_time(
            ( ( two_mbit ? 2 : 1 ) + 4 + earlier.data.size + 3 ) * ( two_mbit ? 4 : 8 ) );

        return time_of( later.when ) - time_of( earlier.when ) - air_time;
    }

    std::chrono::microseconds time_between( const record& earlier, const record& later )
    {
        return std::chrono::microseconds( ( later.when - earlier.when ).usec() );
    }
}
}
