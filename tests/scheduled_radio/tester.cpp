#include "boost_support.hpp"

#include "system_nrf52.h"

#include <iostream>

#include <doctest/doctest.h>

int factorial(int number) { return number <= 1 ? number : factorial(number - 1) * number; }

TEST_CASE("testing the factorial function") {
    CHECK(factorial(1) == 1);
    CHECK(factorial(2) == 2);
    CHECK(factorial(3) == 6);
    CHECK(factorial(10) == 3628800);
}

TEST_CASE("testing the factorial function 2") {
    CHECK(factorial(1) == 1);
    CHECK(factorial(2) == 2);
    CHECK(factorial(3) == 6);
    CHECK(factorial(10) == 3628800);
}

#if 0
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
#endif
