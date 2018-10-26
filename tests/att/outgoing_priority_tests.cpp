#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>
#include <bluetoe/outgoing_priority.hpp>

/*
 * The purpose of this tests is to make sure, that lower_outgoing_priority and higher_outgoing_priority
 * are correctly interpreted. That the notification_queue acts correctly under all curcumstances will
 * be tested within the queue tests.
 */
const std::uint8_t value = 0xAa;

using A = bluetoe::service_uuid16< 0x1234 >;

using A_a = bluetoe::characteristic_uuid16< 0xFF00 >;
using A_b = bluetoe::characteristic_uuid16< 0xFF01 >;
using A_c = bluetoe::characteristic_uuid16< 0xFF02 >;

using B = bluetoe::service_uuid16< 0x1235 >;

using B_a = bluetoe::characteristic_uuid16< 0xFF10 >;
using B_b = bluetoe::characteristic_uuid16< 0xFF11 >;
using B_c = bluetoe::characteristic_uuid16< 0xFF12 >;

template < typename UUID, typename ...Options >
using characteristic =
    bluetoe::characteristic<
        bluetoe::bind_characteristic_value< const std::uint8_t, &value >,
        UUID,
        bluetoe::notify,
        Options...
    >;

using charA_a = characteristic< A_a >;
using charA_b = characteristic< A_b >;
using charA_c = characteristic< A_c >;

using charB_a = characteristic< B_a >;
using charB_b = characteristic< B_b >;
using charB_c = characteristic< B_c >;

using service_A = bluetoe::service<
        A,
        charA_a,
        charA_b
    >;

using service_B = bluetoe::service<
        B,
        charB_a,
        charB_b
    >;

BOOST_AUTO_TEST_CASE( equal_default_priority )
{
    using server   = bluetoe::server< service_A, service_B >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_b >::value ), 0 );
}

BOOST_AUTO_TEST_CASE( raised_priority_in_one_char )
{
    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            charA_c,
            bluetoe::higher_outgoing_priority< A_b >
        >;

    using server   = bluetoe::server< service_A, service_B >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_c >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_c >::value ), 1 );
}

BOOST_AUTO_TEST_CASE( raised_priotity_on_all_services )
{
    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            bluetoe::higher_outgoing_priority< A_a >
        >;

    using service_B = bluetoe::service<
            B,
            charB_a,
            charB_b,
            bluetoe::higher_outgoing_priority< B_a, B_b >
        >;

    using server   = bluetoe::server< service_A, service_B >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_b >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 2 );
}

BOOST_AUTO_TEST_CASE( raised_priority_in_two_chars )
{
    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            charA_c,
            bluetoe::higher_outgoing_priority< A_b, A_a >
        >;

    using server   = bluetoe::server< service_A >;
    using services = server::services;
    using prio    = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_c >::value ), 2 );
}

BOOST_AUTO_TEST_CASE( raised_priority_of_one_service )
{
    using server   = bluetoe::server< service_A, service_B, bluetoe::higher_outgoing_priority< A > >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_b >::value ), 1 );
}

BOOST_AUTO_TEST_CASE( default_prio_of_raise_service_is_higher_than_default_prio_in_other_service )
{
    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            bluetoe::higher_outgoing_priority< A_a >
        >;

    using server   = bluetoe::server< service_A, service_B, bluetoe::higher_outgoing_priority< A > >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 2 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_b >::value ), 2 );
}

BOOST_AUTO_TEST_CASE( no_additional_prio_if_all_chars_of_a_service_have_none_default_prio )
{
    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            bluetoe::higher_outgoing_priority< A_a, A_b >
        >;

    using server   = bluetoe::server< service_A, service_B, bluetoe::higher_outgoing_priority< A > >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 2 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_b >::value ), 2 );
}

// second time, make sure that only characteristics with CCCD count
BOOST_AUTO_TEST_CASE( no_additional_prio_if_all_chars_of_a_service_have_none_default_prio_II )
{
    using charA_b =
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< const std::uint8_t, &value >,
            A_b
        >;

    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            bluetoe::higher_outgoing_priority< A_a >
        >;

    using server   = bluetoe::server< service_A, service_B, bluetoe::higher_outgoing_priority< A > >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_b >::value ), 1 );
}

BOOST_AUTO_TEST_CASE( raised_prio_in_one_char_and_raised_prio_in_other_service )
{
    using service_B = bluetoe::service<
            B,
            charB_a,
            charB_b,
            bluetoe::higher_outgoing_priority< B_a  >
        >;

    using server   = bluetoe::server< service_A, service_B, bluetoe::higher_outgoing_priority< A > >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_b >::value ), 2 );
}

BOOST_AUTO_TEST_CASE( two_services_with_similar_characteristic_priorities_but_different_service_priority )
{
    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            charA_c,
            bluetoe::higher_outgoing_priority< A_a, A_b >
        >;

    using service_B = bluetoe::service<
            B,
            charB_a,
            charB_b,
            charB_c,
            bluetoe::higher_outgoing_priority< B_a >
        >;

    using server   = bluetoe::server< service_A, service_B, bluetoe::higher_outgoing_priority< A > >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_a >::value ), 0 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_b >::value ), 1 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_A, charA_c >::value ), 2 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_a >::value ), 3 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_b >::value ), 4 );
    BOOST_CHECK_EQUAL( int( prio::characteristic_priority< services, service_B, charB_c >::value ), 4 );
}

template < std::size_t I >
using int_c = std::integral_constant< int, I >;

BOOST_AUTO_TEST_CASE( numbers_for_single_priority )
{
    using server   = bluetoe::server< service_A, service_B >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK((
        std::is_same<
            typename prio::numbers< services >::type,
            std::tuple< int_c< 4 > >
        >::value
    ));
}

BOOST_AUTO_TEST_CASE( priority_numbers_for_an_example_with_two_services )
{
    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            bluetoe::higher_outgoing_priority< A_a >
        >;

    using server   = bluetoe::server< service_A, service_B, bluetoe::higher_outgoing_priority< A > >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK((
        std::is_same<
            typename prio::numbers< services >::type,
            std::tuple< int_c< 1 >, int_c< 1 >, int_c< 2 > >
        >::value
    ));
}

BOOST_AUTO_TEST_CASE( priority_numbers_for_two_services_with_similar_characteristic_priorities_but_different_service_priority )
{
    using service_A = bluetoe::service<
            A,
            charA_a,
            charA_b,
            charA_c,
            bluetoe::higher_outgoing_priority< A_a, A_b >
        >;

    using service_B = bluetoe::service<
            B,
            charB_a,
            charB_b,
            charB_c,
            bluetoe::higher_outgoing_priority< B_a >
        >;

    using server   = bluetoe::server< service_A, service_B, bluetoe::higher_outgoing_priority< A > >;
    using services = server::services;
    using prio     = typename server::notification_priority;

    BOOST_CHECK((
        std::is_same<
            typename prio::numbers< services >::type,
            std::tuple< int_c< 1 >, int_c< 1 >, int_c< 1 >, int_c< 1 >, int_c< 2 > >
        >::value
    ));
}


BOOST_AUTO_TEST_CASE( add_prio_tests )
{
    using namespace bluetoe::details;

    BOOST_CHECK((
        std::is_same<
            typename add_prio< std::tuple<>, 0 >::type,
            std::tuple< int_c< 1 > >
        >::value
    ));

    BOOST_CHECK((
        std::is_same<
            typename add_prio< std::tuple< int_c< 3 > >, 0 >::type,
            std::tuple< int_c< 4 > >
        >::value
    ));

    BOOST_CHECK((
        std::is_same<
            typename add_prio< std::tuple<>, 2 >::type,
            std::tuple< int_c< 0 >, int_c< 0 >, int_c< 1 > >
        >::value
    ));

    BOOST_CHECK((
        std::is_same<
            typename add_prio< std::tuple< int_c< 3 >, int_c< 2 > >, 0 >::type,
            std::tuple< int_c< 4 >, int_c< 2 > >
        >::value
    ));

    BOOST_CHECK((
        std::is_same<
            typename add_prio< std::tuple< int_c< 3 >, int_c< 2 >, int_c< 1 > >, 1 >::type,
            std::tuple< int_c< 3 >, int_c< 3 >, int_c< 1 > >
        >::value
    ));

    BOOST_CHECK((
        std::is_same<
            typename add_prio< std::tuple< int_c< 3 >, int_c< 2 >, int_c< 1 > >, 4 >::type,
            std::tuple< int_c< 3 >, int_c< 2 >, int_c< 1 >, int_c< 0 >, int_c< 1 > >
        >::value
    ));
}

