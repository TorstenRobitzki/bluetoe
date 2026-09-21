#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_TESTER_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_TESTER_HPP

/**
 * @file tester.hpp
 *
 * The tester as the radio tests reach it: one connection per test run, opened by the
 * global fixture of dut.cpp on the serial device that BLUETOE_TESTER names, checked for
 * its protocol version and given a fresh session token, like the device under test. The
 * tester is optional: the tests that need none run without it, and a test that needs it
 * is decorated with a precondition on tester_present.
 */

#include "host/proxy.hpp"
#include "host/serial_transport.hpp"
#include "host/tester_functions.hpp"

#include <boost/test/unit_test.hpp>

#include <string>
#include <utility>

namespace bluetoe {
namespace test_rig {

    class tester_connection
    {
    public:
        /**
         * @brief opens the tester BLUETOE_TESTER names and makes it ready for a test
         *
         * @throws rig_error the tester cannot be opened, or it speaks another protocol
         *                   version
         */
        tester_connection();

        template < auto F, typename... Args >
        auto call( Args&&... args )
        {
            return remote_.call< F >( std::forward< Args >( args )... );
        }

        const std::string& implementation_name() const
        {
            return implementation_name_;
        }

        const std::string& build_identifier() const
        {
            return build_identifier_;
        }

    private:
        serial_transport                                transport_;
        proxy< tester_functions, serial_transport >     remote_;
        std::string                                     implementation_name_;
        std::string                                     build_identifier_;
    };

    /**
     * @brief opens the tester if BLUETOE_TESTER names one; for the global fixture of dut.cpp
     */
    void connect_tester_if_named();

    void disconnect_tester();

    /**
     * @brief whether BLUETOE_TESTER named a tester and it is connected
     */
    bool tester_connected();

    /**
     * @brief the tester of this test run
     *
     * @throws rig_error no tester is connected
     */
    tester_connection& the_tester();

    /**
     * @brief predicate for boost::unit_test::precondition: a tester is connected
     */
    struct tester_present
    {
        boost::test_tools::assertion_result operator()( boost::unit_test::test_unit_id ) const;
    };

    /**
     * @brief the decorator of a test that needs the tester: `*if_tester` after its name
     *
     * Without one the test is skipped and reported as such, not passed.
     */
    inline const auto if_tester = boost::unit_test::precondition( tester_present{} );
}
}

#endif
