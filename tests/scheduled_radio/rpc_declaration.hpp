#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RPC_DECLARATION_HPP

#include <bluetoe/link_layer/scheduled_radio2.hpp>

#include "rpc.hpp"

class scheduled_radio :public bluetoe::link_layer::scheduled_radio2<
    bluetoe::link_layer::example_callbacks >
{
public:

};

using tester_calling_iut_rpc_t = decltype( rpc::functions<
    &scheduled_radio::time_now,
    &scheduled_radio::set_access_address_and_crc_init,
    &scheduled_radio::set_phy,
    &scheduled_radio::set_local_address,
    &scheduled_radio::schedule_advertising_event,
    &scheduled_radio::dut_time_accuracy_ppm/*,
    &scheduled_radio::schedule_connection_event,
    &scheduled_radio::cancel_radio_event */ >() );

using iut_calling_tester_rpc_t = decltype( rpc::functions<
    &bluetoe::link_layer::example_callbacks::radio_ready,
    &bluetoe::link_layer::example_callbacks::adv_received,
    &bluetoe::link_layer::example_callbacks::adv_timeout,
    &bluetoe::link_layer::example_callbacks::connection_timeout,
    &bluetoe::link_layer::example_callbacks::connection_end_event,
    &bluetoe::link_layer::example_callbacks::connection_event_canceled,
    &bluetoe::link_layer::example_callbacks::user_timer,
    &bluetoe::link_layer::example_callbacks::user_timer_canceled >() );

#endif
