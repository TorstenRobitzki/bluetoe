/**
 * @file template_dut_rig.cpp
 *
 * What the firmware of a device under test consists of, on any platform: the scheduled
 * radio implementation under test, the platform's serial port, the rig that binds the two,
 * and a main loop. This is the template for a real one: copy it, replace template_radio by
 * the implementation and template_uart by the platform's port, and give the rig the names
 * it reports to the host.
 *
 * This file is not part of any build. The two classes below are placeholders whose
 * requirements are stated once, in the concepts they name, so that this template does not
 * have to follow every change of a concept. The unit tests instantiate the same rig with
 * instrumented dummies, which is what checks the shape.
 */

#include "instrument/dut_rig.hpp"

#ifndef DUT_BUILD_IDENTIFIER
#   define DUT_BUILD_IDENTIFIER "unidentified build"
#endif

namespace template_platform {

    /*
     * The implementation under test. A scheduled radio is a template over the type that
     * receives its callbacks, and reaches that type through the base class relation, as
     * the nRF52 binding does; the rig passes itself. What the class has to provide is
     * stated by scheduled_radio in bluetoe/scheduled_radio2.hpp, and the rig
     * checks it as scheduled_radio< template_radio, rig_t >.
     *
     * An implementation with options binds them here:
     *
     *     template < typename CallBacks >
     *     using template_radio = radio< CallBacks, options... >;
     */
    template < typename CallBacks >
    class template_radio
    {
    };

    /*
     * The platform's serial port, constructed by the rig on its two buffers and on the
     * radio, which it wakes after it received something. What the class has to provide is
     * stated by serial_port in link/serial_port.hpp.
     */
    template < typename Buffer, typename Wake >
    class template_uart
    {
    };

    using rig_t = bluetoe::test_rig::dut_rig< template_radio, template_uart >;

    /*
     * Constructed at startup by the runtime, before main(), so that the session token reads
     * as zero after every reset (instrument.hpp, "Detecting a restart"). A namespace scope
     * object rather than a static in main(): the latter needs the runtime's guards and
     * destructor registration, which a firmware without a C++ runtime does not have.
     */
    rig_t rig( "template radio on no hardware", DUT_BUILD_IDENTIFIER );
}

int main()
{
    for ( ;; )
        template_platform::rig.run();
}
