#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RPC_DECLARATION_HPP

#include "rpc.h"
#include <bluetoe/link_layer/scheduled_radio2.hpp>

using tester_calling_iut_rpc_t = decltype rpc::delare(
    scheduled_radio2::time_now,
    scheduled_radio2::set_local_address,
    scheduled_radio2::schedule_advertising_event
    scheduled_radio2::schedule_connection_event
    scheduled_radio2::cancel_radio_event );

using iut_calling_tester_rpc_t = decltype rpc::delare(
    example_callbacks::radio_ready,
    example_callbacks::adv_received,
    example_callbacks::adv_timeout
    example_callbacks::connection_timeout
    example_callbacks::connection_end_event
    example_callbacks::connection_event_canceled
    example_callbacks::user_timer
    example_callbacks::user_timer_canceled );