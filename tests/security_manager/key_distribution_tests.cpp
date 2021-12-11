#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

struct void_{};

BOOST_FIXTURE_TEST_CASE( no_distribution_over_unsecure_connection, test::legacy_pairing_random_exchanged )
{
    bluetoe::details::link_state< void_ > link( 23 );
    BOOST_CHECK( !connection_data().outgoing_security_manager_data_available( link ) );
}

BOOST_FIXTURE_TEST_CASE( no_distribution_when_in_wrong_state, test::legacy_pairing_confirm_exchanged )
{
    bluetoe::details::link_state< void_ > link( 23 );
    BOOST_CHECK( !connection_data().outgoing_security_manager_data_available( link ) );
}
