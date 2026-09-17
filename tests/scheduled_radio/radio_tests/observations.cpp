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

    std::vector< record > callbacks_of( const std::vector< record >& records, callback_kind kind )
    {
        std::vector< record > result;
        std::copy_if( records.begin(), records.end(), std::back_inserter( result ),
            [ kind ]( const record& r ){ return r.kind == record_kind::callback && r.callback == kind; } );

        return result;
    }

    std::vector< record > calls_of( const std::vector< record >& records, call_kind kind )
    {
        std::vector< record > result;
        std::copy_if( records.begin(), records.end(), std::back_inserter( result ),
            [ kind ]( const record& r ){ return r.kind == record_kind::call && r.call == kind; } );

        return result;
    }

    bool carries( const captured_pdu& p, std::span< const std::uint8_t > bytes )
    {
        return p.data.size == bytes.size()
            && std::equal( bytes.begin(), bytes.end(), p.data.data.begin() );
    }

    long microseconds_between( const captured_pdu& earlier, const captured_pdu& later )
    {
        return std::chrono::duration_cast< std::chrono::microseconds >(
            time_of( later.when ) - time_of( earlier.when ) ).count();
    }
}
}
