#ifndef BLUTOE_TESTS_SCHEDULED_RADIO_SERIALIZE_HPP
#define BLUTOE_TESTS_SCHEDULED_RADIO_SERIALIZE_HPP

#include <bluetoe/link_layer/scheduled_radio2.hpp>

#include "rpc.hpp"

#include  <type_traits>

template < typename IO >
void serialize( IO& io, const bluetoe::link_layer::abs_time& t )
{
    rpc::serialize( io, t.data() );
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::abs_time& t )
{
    bluetoe::link_layer::abs_time::representation_type data;
    rpc::deserialize( io, data );

    t = bluetoe::link_layer::abs_time( data );
}

template < typename IO >
void serialize( IO& io, const bluetoe::link_layer::connection_event_events& evt )
{
    rpc::serialize( io, evt.unacknowledged_data );
    rpc::serialize( io, evt.last_received_not_empty );
    rpc::serialize( io, evt.last_transmitted_not_empty );
    rpc::serialize( io, evt.last_received_had_more_data );
    rpc::serialize( io, evt.pending_outgoing_data );
    rpc::serialize( io, evt.error_occured );
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::connection_event_events& evt )
{
    rpc::deserialize( io, evt.unacknowledged_data );
    rpc::deserialize( io, evt.last_received_not_empty );
    rpc::deserialize( io, evt.last_transmitted_not_empty );
    rpc::deserialize( io, evt.last_received_had_more_data );
    rpc::deserialize( io, evt.pending_outgoing_data );
    rpc::deserialize( io, evt.error_occured );
}

template < typename IO >
void serialize( IO& io, const bluetoe::link_layer::device_address& addr )
{
    rpc::serialize( io, addr.is_random() );
    io.put( addr.begin(), 6u );
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::device_address& addr )
{
    bool is_random = false;
    rpc::deserialize( io, is_random );
    std::uint8_t addr_data[ 6u ];
    io.get( addr_data, sizeof addr_data );

    addr = bluetoe::link_layer::device_address( addr_data, is_random );
}

std::uint8_t* allocate_local_write_buffer( std::uint16_t length );
std::uint8_t* allocate_local_read_buffer( std::uint16_t length );

template < typename IO >
void serialize( IO& io, const bluetoe::link_layer::write_buffer& b )
{
    const std::uint16_t length = b.size;
    rpc::serialize( io, length );
    io.put( b.buffer, length );
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::write_buffer& b )
{
    std::uint16_t length = 0;
    rpc::deserialize( io, length );

    auto buffer = allocate_local_write_buffer( length );
    assert( buffer );
    io.get( buffer, length );

    b = bluetoe::link_layer::write_buffer( buffer, length );
}

template < typename IO >
void serialize( IO& io, const bluetoe::link_layer::read_buffer& b )
{
    const std::uint16_t length = b.size;
    rpc::serialize( io, length );
    io.put( b.buffer, length );
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::read_buffer& b )
{
    std::uint16_t length = 0;
    rpc::deserialize( io, length );

    auto buffer = allocate_local_read_buffer( length );
    assert( buffer );
    io.get( buffer, length );

    b = bluetoe::link_layer::read_buffer( buffer, length );
}

template < typename IO >
void serialize( IO& io, bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t enc )
{
    rpc::serialize( io, static_cast< std::underlying_type_t< bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t > >( enc ) );
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t& enc )
{
    std::underlying_type_t< bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t > raw;
    rpc::deserialize( io, raw );

    enc = static_cast< bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t >( raw );
}

template < typename IO >
void serialize( IO& io, const bluetoe::link_layer::radio_properties& props )
{
    rpc::serialize( io, props.hardware_supports_encryption );
    rpc::serialize( io, props.hardware_supports_lesc_pairing );
    rpc::serialize( io, props.hardware_supports_legacy_pairing );
    rpc::serialize( io, props.hardware_supports_2mbit );
    rpc::serialize( io, props.hardware_supports_synchronized_user_timer );
    rpc::serialize( io, props.radio_max_supported_payload_length );
    rpc::serialize( io, props.sleep_time_accuracy_ppm );
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::radio_properties& props )
{
    rpc::deserialize( io, props.hardware_supports_encryption );
    rpc::deserialize( io, props.hardware_supports_lesc_pairing );
    rpc::deserialize( io, props.hardware_supports_legacy_pairing );
    rpc::deserialize( io, props.hardware_supports_2mbit );
    rpc::deserialize( io, props.hardware_supports_synchronized_user_timer );
    rpc::deserialize( io, props.radio_max_supported_payload_length );
    rpc::deserialize( io, props.sleep_time_accuracy_ppm );
}

#endif
