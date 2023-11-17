#include <bluetoe/ring.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

using ring_t = bluetoe::details::ring< 3u, std::string >;

BOOST_FIXTURE_TEST_CASE( default_empty, ring_t )
{
    std::string d;
    BOOST_CHECK( !try_pop( d ) );
}

BOOST_FIXTURE_TEST_CASE( default_capacity, ring_t )
{
    std::string d;
    BOOST_CHECK( try_push( d ) );
}

BOOST_FIXTURE_TEST_CASE( fill, ring_t )
{
    std::string d1, d2, d3, d4;
    BOOST_CHECK( try_push( d1 ) );
    BOOST_CHECK( try_push( d2 ) );
    BOOST_CHECK( try_push( d3 ) );
    BOOST_CHECK( !try_push( d4 ) );
}

BOOST_FIXTURE_TEST_CASE( push_and_pop, ring_t )
{
    std::string in = "Hallo";
    BOOST_CHECK( try_push( in ) );

    std::string out;
    BOOST_CHECK( try_pop( out ) );

    BOOST_CHECK_EQUAL( in, out );
    BOOST_CHECK( !try_pop( out ) );
}

BOOST_FIXTURE_TEST_CASE( around, ring_t )
{
    std::string d1 = "1", d2 = "2", d3 = "3", d4 = "4";
    std::string out1, out2, out3, out4, out5;

    BOOST_CHECK( try_push( d1 ) );
    BOOST_CHECK( try_push( d2 ) );
    BOOST_CHECK( try_push( d3 ) );
    BOOST_CHECK( try_pop( out1 ) );
    BOOST_CHECK( try_push( d4 ) );
    BOOST_CHECK( try_pop( out2 ) );
    BOOST_CHECK( try_pop( out3 ) );
    BOOST_CHECK( try_pop( out4 ) );
    BOOST_CHECK( !try_pop( out5 ) );

    BOOST_CHECK_EQUAL( d1, out1 );
    BOOST_CHECK_EQUAL( d2, out2 );
    BOOST_CHECK_EQUAL( d3, out3 );
    BOOST_CHECK_EQUAL( d4, out4 );
}
