#include "radio_tests/observations.hpp"

#include "host/tester_time.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>

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

    std::chrono::microseconds time_between( const captured_pdu& earlier, const captured_pdu& later )
    {
        return std::chrono::duration_cast< std::chrono::microseconds >(
            time_of( later.when ) - time_of( earlier.when ) );
    }

    std::chrono::microseconds time_between( const record& earlier, const record& later )
    {
        return std::chrono::microseconds( ( later.when - earlier.when ).usec() );
    }
}
}
