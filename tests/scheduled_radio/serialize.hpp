#ifndef BLUTOE_TESTS_SCHEDULED_RADIO_SERIALIZE_HPP
#define BLUTOE_TESTS_SCHEDULED_RADIO_SERIALIZE_HPP

#include <bluetoe/link_layer/scheduled_radio2.hpp>

#include "rpc.hpp"

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
    rpc::serialize( io, addr.begin(), 6u );
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

#endif
