/**
 * @file tester.cpp
 *
 * The tester: the rig of instrument/tester_rig.hpp on an nRF52 development kit, over the
 * UART the devices under test use as well, with the reset line of platform.hpp to the device
 * under test (decisions 4 and 23).
 */

#include "instrument/tester_rig.hpp"
#include "nrf52/uart.hpp"
#include "tester/platform.hpp"

#ifndef TESTER_BUILD_IDENTIFIER
#   define TESTER_BUILD_IDENTIFIER "unidentified build"
#endif

/*
 * A named namespace: with an anonymous one the rig's type has internal linkage, and GCC's
 * visibility check objects to its members.
 */
namespace bluetoe_tester {

    using rig_t = bluetoe::test_rig::tester_rig<
        bluetoe::test_rig::nrf52::uart,
        bluetoe::test_rig::platform >;

    /*
     * Constructed at startup by the runtime, before main(), so that the session token reads
     * as zero after every reset (instrument.hpp, "Detecting a restart"); a namespace scope
     * object for the reasons dut_rigs/template_dut_rig.cpp gives.
     */
    rig_t rig( "tester on nRF52840-DK", TESTER_BUILD_IDENTIFIER );
}

int main()
{
    for ( ;; )
        bluetoe_tester::rig.run();
}
