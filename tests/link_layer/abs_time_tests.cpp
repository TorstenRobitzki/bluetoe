#include <bluetoe/abs_time.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( default_ctor )
{
    const bluetoe::link_layer::abs_time time;

    BOOST_CHECK_EQUAL( time.data(), 0 );
    BOOST_CHECK_EQUAL( time, bluetoe::link_layer::abs_time( 0 ) );
}

BOOST_AUTO_TEST_CASE( rep_ctor )
{
    const bluetoe::link_layer::abs_time t1( 0xffffffff );
    const bluetoe::link_layer::abs_time t2( 0 );
    const bluetoe::link_layer::abs_time t3( 424242 );

    BOOST_CHECK_EQUAL( t1.data(), 0xffffffff );
    BOOST_CHECK_EQUAL( t2.data(), 0 );
    BOOST_CHECK_EQUAL( t3.data(), 424242 );
}

BOOST_AUTO_TEST_CASE( adding_delta )
{
    bluetoe::link_layer::abs_time t( 0 );
    t += bluetoe::link_layer::delta_time::msec( 1 );

    BOOST_CHECK_EQUAL( t.data(), 1000 );
}

BOOST_AUTO_TEST_CASE( substracting_delta )
{
    bluetoe::link_layer::abs_time t( 3000 );
    t -= bluetoe::link_layer::delta_time::msec( 1 );

    BOOST_CHECK_EQUAL( t.data(), 2000 );
}

BOOST_AUTO_TEST_CASE( less_than_simple )
{
    bluetoe::link_layer::abs_time t1( 0 );
    bluetoe::link_layer::abs_time t2( 1 );
    bluetoe::link_layer::abs_time t3( 1 );

    BOOST_CHECK_LT( t1, t2 );
    BOOST_CHECK( !(t2 < t1) );
    BOOST_CHECK( !(t2 < t3) );
}

BOOST_AUTO_TEST_CASE( less_than_overflow )
{
    for ( const auto t : std::initializer_list< bluetoe::link_layer::abs_time >{
        bluetoe::link_layer::abs_time( 0 ),
        bluetoe::link_layer::abs_time( 0x100 ),
        bluetoe::link_layer::abs_time( 0xFFFFFFFF ),
        bluetoe::link_layer::abs_time( 0x80000000 ),
        bluetoe::link_layer::abs_time( 0x10000000 )
    } )
    {
        const auto t2 = t + bluetoe::link_layer::delta_time( bluetoe::link_layer::abs_time::max_distance - 1 );
        const auto t3 = t + bluetoe::link_layer::delta_time( bluetoe::link_layer::abs_time::max_distance + 1 );
        const auto t4 = t + bluetoe::link_layer::delta_time( 1 );
        BOOST_CHECK_LT( t, t2 );
        BOOST_CHECK_LT( t, t4 );
        BOOST_CHECK(!( t2 < t ));
        BOOST_CHECK(!( t < t ));
        BOOST_CHECK(!( t2 < t2 ));
        BOOST_CHECK( !(t < t3) );
        BOOST_CHECK(!( t3 < t3 ));
    }
}

BOOST_AUTO_TEST_CASE( less_equal )
{
    bluetoe::link_layer::abs_time t1( 0xFFFFFFFF );
    bluetoe::link_layer::abs_time t2( 0xFFFFFFFF );
    bluetoe::link_layer::abs_time t3( 0 );

    BOOST_CHECK_LE( t1, t2 );
    BOOST_CHECK_LE( t2, t1 );
    BOOST_CHECK_LE( t1, t3 );
    BOOST_CHECK( !( t3 <= t1 ) );
}

BOOST_AUTO_TEST_CASE( greater_than )
{
    bluetoe::link_layer::abs_time t1( 5 );
    bluetoe::link_layer::abs_time t2( 10 );
    bluetoe::link_layer::abs_time t3( 10 );
    bluetoe::link_layer::abs_time t4( 0xFFFFFFFF );

    BOOST_CHECK_GT( t2, t1 );
    BOOST_CHECK( !( t1 > t2 ) );
    BOOST_CHECK( !( t2 > t3 ) );

    BOOST_CHECK_GT( t1, t4 );
}

BOOST_AUTO_TEST_CASE( greater_than_equal )
{
    bluetoe::link_layer::abs_time t1( 5 );
    bluetoe::link_layer::abs_time t2( 10 );
    bluetoe::link_layer::abs_time t3( 10 );
    bluetoe::link_layer::abs_time t4( 0xFFFFFFFF );

    BOOST_CHECK_GE( t2, t1 );
    BOOST_CHECK_GE( t2, t3 );
    BOOST_CHECK( !( t1 >= t2 ) );

    BOOST_CHECK_GE( t1, t4 );
}

BOOST_AUTO_TEST_CASE( equal )
{
    bluetoe::link_layer::abs_time t1( 5 );
    bluetoe::link_layer::abs_time t2( 10 );
    bluetoe::link_layer::abs_time t3( 10 );
    bluetoe::link_layer::abs_time t4( 0xFFFFFFFF );

    BOOST_CHECK( !( t1 == t2 ) );
    BOOST_CHECK_EQUAL( t2, t3 );
    BOOST_CHECK( !( t1 == t4 ) );
}

BOOST_AUTO_TEST_CASE( print )
{
    bluetoe::link_layer::abs_time t1(
        4 * 60 * 1000 * 1000
      +     33 * 1000 * 1000
      +            45 * 1000
      +                  123);

    std::stringstream out;
    t1.print(out);

    BOOST_CHECK_EQUAL( out.str(), "4:33.045.123" );
}

BOOST_AUTO_TEST_CASE( addition )
{
    bluetoe::link_layer::abs_time t1( 5 );
    bluetoe::link_layer::abs_time t2( 0xFFFFFFFF );
    bluetoe::link_layer::delta_time d1( 6 );
    bluetoe::link_layer::delta_time d2( 0 );

    BOOST_CHECK_EQUAL( t1 + d1, bluetoe::link_layer::abs_time( 11 ) );
    BOOST_CHECK_EQUAL( d1 + t1, bluetoe::link_layer::abs_time( 11 ) );
    BOOST_CHECK_EQUAL( t2 + d1, bluetoe::link_layer::abs_time( 5 ) );
    BOOST_CHECK_EQUAL( d1 + t2, bluetoe::link_layer::abs_time( 5 ) );

    BOOST_CHECK_EQUAL( t1 + d2, t1 );
    BOOST_CHECK_EQUAL( d2 + t1, t1 );
    BOOST_CHECK_EQUAL( t2 + d2, t2 );
    BOOST_CHECK_EQUAL( d2 + t2, t2 );
}

BOOST_AUTO_TEST_CASE( substraction )
{
    bluetoe::link_layer::abs_time t1( 5 );
    bluetoe::link_layer::abs_time t2( 0xFFFFFFFF );

    BOOST_CHECK_EQUAL( t1 - t2, bluetoe::link_layer::delta_time( 6 ) );
    BOOST_CHECK_EQUAL( t2 - t1, bluetoe::link_layer::delta_time( 0xFFFFFFFA ) );
}

BOOST_AUTO_TEST_CASE( substraction_delta )
{
    bluetoe::link_layer::abs_time t1( 5 );
    bluetoe::link_layer::abs_time t2( 0xFFFFFFFF );
    bluetoe::link_layer::delta_time d1( 6 );
    bluetoe::link_layer::delta_time d2( 0 );

    BOOST_CHECK_EQUAL( t1 - d1, t2 );
    BOOST_CHECK_EQUAL( t2 - d1, bluetoe::link_layer::abs_time( 0xFFFFFFF9 ) );
    BOOST_CHECK_EQUAL( t1 - d2, t1 );
    BOOST_CHECK_EQUAL( t2 - d2, t2 );
}
