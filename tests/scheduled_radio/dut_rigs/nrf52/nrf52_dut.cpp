/**
 * @file nrf52_dut.cpp
 *
 * The device under test on the nRF52 development kits: the rig around the platform's
 * scheduled radio without options, over the UART. This is instrument/template_dut_rig.cpp
 * with the two types and the names filled in (decision 22).
 */

#include "dut_rigs/nrf52/uart.hpp"
#include "instrument/dut_rig.hpp"

#include <bluetoe/radio.hpp>

#ifndef DUT_BUILD_IDENTIFIER
#   define DUT_BUILD_IDENTIFIER "unidentified build"
#endif

/*
 * A named namespace: with an anonymous one the rig's type has internal linkage, and GCC's
 * visibility check objects to its members.
 */
namespace nrf52_dut {

    /*
     * The configuration this rig tests: the radio as it comes.
     */
    template < typename CallBacks >
    using radio = bluetoe::radio< CallBacks >;

    using rig_t = bluetoe::test_rig::dut_rig< radio, bluetoe::test_rig::nrf52::uart >;

    /*
     * Constructed at startup by the runtime, before main(), so that the session token reads
     * as zero after every reset (instrument.hpp, "Detecting a restart"). A namespace scope
     * object rather than a static in main(): the latter needs the runtime's guards and
     * destructor registration, which a firmware without a C++ runtime does not have.
     */
    rig_t rig( "nrf52_radio on nRF52840-DK", DUT_BUILD_IDENTIFIER );
}

int main()
{
    for ( ;; )
        nrf52_dut::rig.run();
}
