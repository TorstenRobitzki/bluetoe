#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_DUT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_DUT_HPP

/**
 * @file dut.hpp
 *
 * The device under test as the radio tests reach it: one connection per test run, opened
 * by a global fixture on the serial device that BLUETOE_DUT names, checked for its
 * protocol version, given a fresh session token and asked for its properties once. Every
 * test reaches it through dut_fixture; a test that needs a feature the device may lack is
 * decorated with a precondition on dut_supports.
 *
 * BLUETOE_DUT_TIMEOUT_MS bounds one request; the default covers a point multiplication on
 * a small core.
 */

#include "host/dut_functions.hpp"
#include "host/proxy.hpp"
#include "host/serial_transport.hpp"

#include <bluetoe/radio_properties.hpp>

#include <boost/test/unit_test.hpp>

#include <string>
#include <utility>

namespace bluetoe {
namespace test_rig {

    class dut_connection
    {
    public:
        /**
         * @brief opens the device BLUETOE_DUT names and makes it ready for a test
         *
         * A device that was used by an earlier run still carries that run's session
         * token; the first response tells which, and the connection adopts it before it
         * sets its own.
         *
         * @throws rig_error the variable is not set, the device cannot be opened, or it
         *                   speaks another protocol version
         */
        dut_connection();

        template < auto F, typename... Args >
        auto call( Args&&... args )
        {
            return remote_.call< F >( std::forward< Args >( args )... );
        }

        const link_layer::radio_properties& properties() const
        {
            return properties_;
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
        serial_transport                            transport_;
        proxy< dut_functions, serial_transport >    remote_;
        std::string                                 implementation_name_;
        std::string                                 build_identifier_;
        link_layer::radio_properties                properties_;
    };

    /**
     * @brief the connection of this test run
     */
    dut_connection& the_dut();

    struct dut_fixture
    {
        dut_connection& device = the_dut();
    };

    /**
     * @brief predicate for boost::unit_test::precondition: the device has a feature
     *
     * @code
     * BOOST_AUTO_TEST_SUITE( pairing_toolbox,
     *     *boost::unit_test::precondition( dut_supports{ &radio_properties::hardware_supports_lesc_pairing } ) )
     * @endcode
     */
    struct dut_supports
    {
        bool link_layer::radio_properties::* feature;

        boost::test_tools::assertion_result operator()( boost::unit_test::test_unit_id ) const;
    };
}
}

#endif
