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
std::uint8_t value_a = 0;
std::uint8_t value_b = 0;


template < class Server >
void check_priorities( Server& server, std::initializer_list< std::uint8_t* > by_priority )
{
    for ( auto characteristic : by_priority )
        server.notify( *characteristic );


}

using server_with_higher_prio = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x0815 >,
        bluetoe::higher_outgoing_priority<
            bluetoe::characteristic_uuid16< 0x0001 >
        >,
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< std::uint8_t, &value_a >,
            bluetoe::characteristic_uuid16< 0x0001 >,
            bluetoe::notify
        >,
        bluetoe::characteristic<
            bluetoe::bind_characteristic_value< std::uint8_t, &value_b >,
            bluetoe::characteristic_uuid16< 0x0002 >,
            bluetoe::notify
        >
    >
>;

BOOST_FIXTURE_TEST_CASE( raised_priority_in_char, server_with_higher_prio )
{
    check_priorities( *this, { &value_a, &value_b } );
}