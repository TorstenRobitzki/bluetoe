#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

struct connected {};

BOOST_FIXTURE_TEST_CASE( respond_with_an_unknown_rsp, connected )
{
}

BOOST_FIXTURE_TEST_CASE( responding_in_feature_setup, connected )
{
}

BOOST_FIXTURE_TEST_CASE( respond_to_a_version_ind, connected )
{
}

BOOST_FIXTURE_TEST_CASE( respond_to_a_ping, connected )
{
}

BOOST_FIXTURE_TEST_CASE( accept_termination, connected )
{
}