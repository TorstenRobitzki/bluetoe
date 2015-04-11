#include "test_services.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace {

}

BOOST_AUTO_TEST_CASE( service_without_any_characteristic_results_in_one_attribute )
{
    const auto number = empty_service::number_of_attributes;
    BOOST_CHECK_EQUAL( 1u, number );
}
