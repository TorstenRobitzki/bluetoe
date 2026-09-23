/**
 * @file nrf52_dut_lfxo.cpp
 *
 * The device under test on the nRF52 development kits with the 32.768 kHz crystal as the
 * sleep clock: nrf52_dut.cpp with that option.
 */

#include "instrument/dut_rig.hpp"
#include "nrf52/uart.hpp"

#include <bluetoe/radio.hpp>

#ifndef DUT_BUILD_IDENTIFIER
#   define DUT_BUILD_IDENTIFIER "unidentified build"
#endif

/*
 * A named namespace: with an anonymous one the rig's type has internal linkage, and GCC's
 * visibility check objects to its members.
 */
namespace nrf52_dut_lfxo {

    /*
     * The configuration this rig tests: the radio that encrypts, on the 32.768 kHz crystal
     * as its sleep clock, the high frequency crystal started before every event and stopped
     * after it.
     */
    template < typename CallBacks >
    using radio = bluetoe::radio< CallBacks, bluetoe::nrf52_details::encrypting, bluetoe::nrf::sleep_clock_crystal_oscillator >;

    using rig_t = bluetoe::test_rig::dut_rig< radio, bluetoe::test_rig::nrf52::uart >;

    /*
     * Constructed at startup by the runtime, before main(), so that the session token reads
     * as zero after every reset. A namespace scope
     * object rather than a static in main(): the latter needs the runtime's guards and
     * destructor registration, which a firmware without a C++ runtime does not have.
     */
    rig_t rig( "nrf52_radio, encrypting, crystal sleep clock, on nRF52840-DK", DUT_BUILD_IDENTIFIER );
}

int main()
{
    for ( ;; )
        nrf52_dut_lfxo::rig.run();
}
