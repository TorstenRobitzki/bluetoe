#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_TESTER_RIG_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_TESTER_RIG_HPP

/**
 * @file tester_rig.hpp
 *
 * The platform independent half of the tester: what the contract in
 * tests/scheduled_radio/tester.hpp asks for that is neither the port nor the pin, on top
 * of what instrument/instrument.hpp provides to both instruments. See
 * documentation/scheduled_radio_test_rig.md, decisions 4 and 22.
 *
 * The first thing the tester can do is reset the device under test. The radio, the
 * programs and the queue of received PDUs follow, with decision 11, step 3.
 *
 * The host instantiates the same template with dummies (host/tester_functions.hpp) to
 * obtain the function list the wire is keyed on, which is why no function of the list
 * carries a type of the platform.
 */

#include "instrument/instrument.hpp"
#include "link/frame.hpp"
#include "link/function_list.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief version of the wire protocol of the tester
     *
     * Counts the function lists a firmware was built with: changes whenever
     * tester_rig::functions changes after a tester was flashed with the current one.
     */
    constexpr std::uint16_t tester_protocol_version = 1;

    /**
     * @brief what a platform provides to the tester besides the serial port
     *
     * The rig gets its idling and its wake-up from the radio under test; the tester has
     * no radio yet, and the reset line is a pin the tester owns (decision 4). Both come
     * from the platform.
     */
    template < typename T >
    concept tester_platform = requires ( T platform )
    {
        /*
         * Holds the reset input of the device under test asserted for as long as that
         * part needs, then releases it. Blocks for that long.
         */
        platform.reset_device_under_test();

        /*
         * Returns once wake_up() was called, and may return earlier.
         */
        platform.run();

        /*
         * Callable from any context, including the port's.
         */
        platform.wake_up();
    };

    /**
     * @brief the rig that is the tester
     *
     * Port is the platform's serial port, a template over the buffer type and the type
     * it wakes, which is the tester. Platform is what else the tester needs from the
     * hardware, see tester_platform; it is constructed by the tester, and nothing about
     * it reaches the wire.
     */
    template <
        template < typename Buffer, typename Wake > class Port,
        typename Platform,
        std::size_t MaxPayload = default_max_payload >
    class tester_rig : public instrument< tester_rig< Port, Platform, MaxPayload >, Port, MaxPayload >
    {
    public:
        using instrument_t = instrument< tester_rig, Port, MaxPayload >;

        tester_rig( std::string_view implementation_name, std::string_view build_identifier )
            : instrument_t( implementation_name, build_identifier )
            , platform_()
        {
            static_assert( tester_platform< Platform > );

            instrument_t::start();
        }

        /**
         * @brief one iteration of the main loop: answer a buffered request, then idle
         */
        void run()
        {
            instrument_t::serve();
            platform_.run();
        }

        void wake_up()
        {
            platform_.wake_up();
        }

        /**
         * @name Instrument functions
         *
         * The functions of instrument.hpp and tester.hpp that are the tester's own; the
         * names and the session token are the instrument's.
         * @{
         */
        std::uint16_t protocol_version() const
        {
            return tester_protocol_version;
        }

        /**
         * @brief hold the reset input of the device under test asserted, then release it
         *
         * The host does not wait a fixed time afterwards: it polls until the device
         * answers and requires the session token it set before the reset to read zero.
         */
        void reset_device_under_test()
        {
            platform_.reset_device_under_test();
        }
        /** @} */

        using functions = function_list<
            &tester_rig::protocol_version,
            &tester_rig::implementation_name,
            &tester_rig::build_identifier,
            &tester_rig::set_session_token,
            &tester_rig::reset_device_under_test >;

    private:
        Platform    platform_;
    };
}
}

#endif
