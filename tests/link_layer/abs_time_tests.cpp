#include <bluetoe/abs_time.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( default_ctor )
{
    const bluetoe::link_layer::abs_time time;

    BOOST_CHECK_EQUAL( time.data(), 0 );
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

BOOST_AUTO_TEST_CASE( in_near_past_simple )
{
    const bluetoe::link_layer::abs_time t1( 0 );
    const bluetoe::link_layer::abs_time t2( 1 );

    BOOST_CHECK( t1.is_in_near_past( t2 ) );
    BOOST_CHECK( !t2.is_in_near_past( t1 ) );
}

BOOST_AUTO_TEST_CASE( a_time_is_not_in_its_own_near_past )
{
    const bluetoe::link_layer::abs_time t( 424242 );

    BOOST_CHECK( !t.is_in_near_past( t ) );
}

BOOST_AUTO_TEST_CASE( in_near_past_over_the_whole_ring )
{
    for ( const auto t : std::initializer_list< bluetoe::link_layer::abs_time >{
        bluetoe::link_layer::abs_time( 0 ),
        bluetoe::link_layer::abs_time( 0x100 ),
        bluetoe::link_layer::abs_time( 0xFFFFFFFF ),
        bluetoe::link_layer::abs_time( 0x80000000 ),
        bluetoe::link_layer::abs_time( 0x10000000 )
    } )
    {
        const auto just_after   = t + bluetoe::link_layer::delta_time( 1 );
        const auto within       = t + bluetoe::link_layer::delta_time( bluetoe::link_layer::abs_time::max_distance - 1 );
        const auto beyond       = t + bluetoe::link_layer::delta_time( bluetoe::link_layer::abs_time::max_distance + 1 );

        BOOST_CHECK( t.is_in_near_past( just_after ) );
        BOOST_CHECK( t.is_in_near_past( within ) );

        // beyond the window the answer is "neither just gone nor imminent"
        BOOST_CHECK( !t.is_in_near_past( beyond ) );

        BOOST_CHECK( !within.is_in_near_past( t ) );
        BOOST_CHECK( !t.is_in_near_past( t ) );
        BOOST_CHECK( !within.is_in_near_past( within ) );
    }
}

/*
 * A reference far away in either direction answers false. That is ordinary use and not a
 * violated precondition: such a time is neither just gone nor about to happen.
 */
BOOST_AUTO_TEST_CASE( a_far_away_reference_is_not_near )
{
    const bluetoe::link_layer::abs_time now( 0x40000000 );
    const auto far_ahead  = now + bluetoe::link_layer::delta_time( 10 * bluetoe::link_layer::abs_time::max_distance );
    const auto far_behind = now - bluetoe::link_layer::delta_time( 10 * bluetoe::link_layer::abs_time::max_distance );

    BOOST_CHECK( !now.is_in_near_past( far_ahead ) );
    BOOST_CHECK( !now.is_in_near_past( far_behind ) );
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

    BOOST_CHECK_EQUAL( ( t1 + d1 ).data(), 11u );
    BOOST_CHECK_EQUAL( ( d1 + t1 ).data(), 11u );
    BOOST_CHECK_EQUAL( ( t2 + d1 ).data(), 5u );
    BOOST_CHECK_EQUAL( ( d1 + t2 ).data(), 5u );

    BOOST_CHECK_EQUAL( ( t1 + d2 ).data(), t1.data() );
    BOOST_CHECK_EQUAL( ( d2 + t1 ).data(), t1.data() );
    BOOST_CHECK_EQUAL( ( t2 + d2 ).data(), t2.data() );
    BOOST_CHECK_EQUAL( ( d2 + t2 ).data(), t2.data() );
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

    BOOST_CHECK_EQUAL( ( t1 - d1 ).data(), t2.data() );
    BOOST_CHECK_EQUAL( ( t2 - d1 ).data(), 0xFFFFFFF9 );
    BOOST_CHECK_EQUAL( ( t1 - d2 ).data(), t1.data() );
    BOOST_CHECK_EQUAL( ( t2 - d2 ).data(), t2.data() );
}
