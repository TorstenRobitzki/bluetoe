#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

struct void_{};

BOOST_AUTO_TEST_CASE_TEMPLATE( no_distribution_over_unsecure_connection,  Manager, test::legacy_managers )
{
    test::legacy_pairing_random_exchanged< Manager > fixture;

    bluetoe::details::link_state< void_ > link( 23 );
    BOOST_CHECK( !fixture.connection_data().outgoing_security_manager_data_available( link ) );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( no_distribution_when_in_wrong_state, Manager, test::legacy_managers )
{
    test::legacy_pairing_confirm_exchanged< Manager > fixture;

    bluetoe::details::link_state< void_ > link( 23 );
    BOOST_CHECK( !fixture.connection_data().outgoing_security_manager_data_available( link ) );
}
