#ifndef BLUTOE_TESTS_SCHEDULED_RADIO_SERIALIZE_HPP
#define BLUTOE_TESTS_SCHEDULED_RADIO_SERIALIZE_HPP

#include <bluetoe/link_layer/scheduled_radio2.hpp>

#include "rpc.hpp"

template < typename IO >
void serialize( IO& io, const bluetoe::link_layer::abs_time& )
{
    // TODO
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::abs_time& )
{
    // TODO
}

template < typename IO >
void serialize( IO& io, const bluetoe::link_layer::connection_event_events& )
{
    // TODO
}

template < typename IO >
void deserialize( IO& io, bluetoe::link_layer::connection_event_events& )
{
    // TODO
}

#endif
