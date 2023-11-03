#include "serialize.hpp"
#include "nrf_uart_stream.hpp"

#include <bluetoe/device.hpp>

// TODO should be hidden with something similar to bluetoe::device
using scheduled_radio = bluetoe::nrf52_radio< true >::template radio_t< bluetoe::link_layer::example_callbacks >;

#include "rpc_declaration.hpp"


auto protocol = rpc::protocol(
    iut_calling_tester_rpc_t(), tester_calling_iut_rpc_t() );

using stream_t = nrf_uart_stream< 29, 28, 31, 30 >;

stream_t io;

/*
 * Forward the callback calls to the lower tester
 */
void bluetoe::link_layer::example_callbacks::radio_ready( bluetoe::link_layer::abs_time now )
{
    protocol.call< &bluetoe::link_layer::example_callbacks::radio_ready >( io, now );
}

void bluetoe::link_layer::example_callbacks::adv_received( bluetoe::link_layer::abs_time when, const read_buffer& response )
{
    protocol.call< &bluetoe::link_layer::example_callbacks::adv_received >( io, when, response );
}

void bluetoe::link_layer::example_callbacks::adv_timeout( bluetoe::link_layer::abs_time now )
{
    protocol.call< &bluetoe::link_layer::example_callbacks::radio_ready >( io, now );
}

void bluetoe::link_layer::example_callbacks::connection_timeout( bluetoe::link_layer::abs_time now )
{
    protocol.call< &bluetoe::link_layer::example_callbacks::radio_ready >( io, now );
}

void bluetoe::link_layer::example_callbacks::connection_end_event( bluetoe::link_layer::abs_time when, connection_event_events evts )
{
    protocol.call< &bluetoe::link_layer::example_callbacks::connection_end_event >( io, when, evts );
}

void bluetoe::link_layer::example_callbacks::connection_event_canceled( bluetoe::link_layer::abs_time now )
{
    protocol.call< &bluetoe::link_layer::example_callbacks::connection_event_canceled >( io, now );
}

void bluetoe::link_layer::example_callbacks::user_timer( bluetoe::link_layer::abs_time when )
{
    protocol.call< &bluetoe::link_layer::example_callbacks::user_timer >( io, when );
}

void bluetoe::link_layer::example_callbacks::user_timer_canceled()
{
    protocol.call< &bluetoe::link_layer::example_callbacks::user_timer_canceled >( io );
}

scheduled_radio radio;

int main()
{
    // This probably invokes UB
    protocol.register_implementation( radio );

    for ( ;; )
        protocol.deserialize_call( io );
}
