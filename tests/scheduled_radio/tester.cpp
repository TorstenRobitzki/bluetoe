


/**
 * @test using schedule_advertising_event(), make sure, that advertising comes
 *       at the right channel with the correct data and at the correct time.
 *
 * Asking for two advertisments to be able to use the first advertisment as reference
 */
static bool test_schedule_advertising_event_timing_and_data(remote_t& con, tester_t tester )
{
    // register a set of callbacks to be called from
    registered_callbacks cbs( con );

    const auto now = con.call( scheduled_radio2::time_now );

    const auto first = now + 1000u;
    const auto second = first + 1000u;

    con.call( scheduled_radio2::device_address, local_address );
    const bool rc_first_advertisment = con.call(
        scheduled_radio2::schedule_advertising_event,
        37u,
        first,
        advertising_data,
        response_data,
        receive );

    const bool rc_second_advertisment = con.call(
        scheduled_radio2::schedule_advertising_event,
        37u,
        second,
        advertising_data,
        response_data,
        receive );
}

int main()
{

}