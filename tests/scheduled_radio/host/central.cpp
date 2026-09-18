#include "host/central.hpp"

#include <algorithm>
#include <cassert>

namespace bluetoe {
namespace test_rig {

    namespace {
        constexpr std::uint8_t nesn_mask    = 0x04;
        constexpr std::uint8_t sn_mask      = 0x08;
        constexpr std::uint8_t md_mask      = 0x10;

        std::vector< std::uint8_t > data_channel_pdu(
            llid kind, bool sn, bool nesn, bool more_data, std::span< const std::uint8_t > payload )
        {
            std::vector< std::uint8_t > result( 2 + payload.size() );
            result[ 0 ] = static_cast< std::uint8_t >( kind )
                | ( nesn ? nesn_mask : 0 )
                | ( sn ? sn_mask : 0 )
                | ( more_data ? md_mask : 0 );
            result[ 1 ] = static_cast< std::uint8_t >( payload.size() );
            std::copy( payload.begin(), payload.end(), result.begin() + 2 );

            return result;
        }
    }

    std::vector< std::uint8_t > central::send( std::span< const std::uint8_t > payload, bool more_data, llid kind )
    {
        if ( sent_ )
        {
            sn_   = !sn_;
            nesn_ = !nesn_;
        }

        return build( payload, more_data, kind );
    }

    std::vector< std::uint8_t > central::resend()
    {
        assert( sent_ );

        nesn_ = !nesn_;

        last_[ 0 ] = ( last_[ 0 ] & ~nesn_mask ) | ( nesn_ ? nesn_mask : 0 );

        return last_;
    }

    std::vector< std::uint8_t > central::send_nack( std::span< const std::uint8_t > payload, bool more_data, llid kind )
    {
        assert( sent_ );

        sn_ = !sn_;

        return build( payload, more_data, kind );
    }

    std::vector< std::uint8_t > central::build( std::span< const std::uint8_t > payload, bool more_data, llid kind )
    {
        sent_ = true;
        last_ = data_channel_pdu( kind, sn_, nesn_, more_data, payload );

        return last_;
    }

    bool sequence_number( std::span< const std::uint8_t > pdu )
    {
        return pdu[ 0 ] & sn_mask;
    }

    bool next_expected_sequence_number( std::span< const std::uint8_t > pdu )
    {
        return pdu[ 0 ] & nesn_mask;
    }

    bool more_data( std::span< const std::uint8_t > pdu )
    {
        return pdu[ 0 ] & md_mask;
    }

    std::vector< std::uint8_t > reply_to(
        std::span< const std::uint8_t > sent, std::span< const std::uint8_t > payload, bool more_data, llid kind )
    {
        return data_channel_pdu( kind, next_expected_sequence_number( sent ), !sequence_number( sent ), more_data, payload );
    }

    bool acknowledges( const captured_pdu& reply, std::span< const std::uint8_t > sent )
    {
        const std::span< const std::uint8_t > bytes( reply.data.data.data(), reply.data.size );

        return next_expected_sequence_number( bytes ) != sequence_number( sent );
    }

    bool is_new( const captured_pdu& reply, std::span< const std::uint8_t > sent )
    {
        const std::span< const std::uint8_t > bytes( reply.data.data.data(), reply.data.size );

        return sequence_number( bytes ) == next_expected_sequence_number( sent );
    }
}
}
