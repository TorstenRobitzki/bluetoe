#include <iostream>
#include <bluetoe/link_layer/notification_queue.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

using queue17 = bluetoe::link_layer::notification_queue< 17u >;
using queue3 = bluetoe::link_layer::notification_queue< 3u >;
using queue8 = bluetoe::link_layer::notification_queue< 8u >;

BOOST_AUTO_TEST_SUITE( notifications )

    BOOST_FIXTURE_TEST_CASE( empty_queue, queue17 )
    {
        BOOST_CHECK( dequeue_indication_or_confirmation().first == empty );
    }

    BOOST_FIXTURE_TEST_CASE( adding_one_notification, queue17 )
    {
        queue_notification( 12u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 12u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( dequed_elements_are_removed, queue17 )
    {
        queue_notification( 12u );
        dequeue_indication_or_confirmation();
        BOOST_CHECK( dequeue_indication_or_confirmation().first == empty );
    }

    BOOST_FIXTURE_TEST_CASE( adding_two_notification, queue17 )
    {
        queue_notification( 12u );
        queue_notification( 16u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 12u } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 16u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( adding_notification_twice, queue17 )
    {
        queue_notification( 12u );
        queue_notification( 12u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 12u } ) );
        BOOST_CHECK( dequeue_indication_or_confirmation().first == empty );
    }

    /*
     * The queue should behave a little bit like a queue and thus not always deliver the same element
     */
    BOOST_FIXTURE_TEST_CASE( queue_like, queue17 )
    {
        queue_notification( 12u );
        queue_notification( 16u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 12u } ) );

        queue_notification( 12u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 16u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( wrap_around, queue17 )
    {
        queue_notification( 16u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 16u } ) );

        queue_notification( 0u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 0u } ) );

        BOOST_CHECK( dequeue_indication_or_confirmation().first == empty );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( indications )

    BOOST_FIXTURE_TEST_CASE( queue_an_indication, queue3 )
    {
        queue_indication( 2 );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ indication, 2u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( mixed_notification_and_indications, queue3 )
    {
        queue_notification( 2 );
        queue_indication( 2 );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ indication, 2u } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 2u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( no_indication_until_confirmed, queue17 )
    {
        queue_notification( 12u );
        queue_indication( 2u );
        queue_indication( 16u );

        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ indication, 2u } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ notification, 12u } ) );
        BOOST_CHECK( dequeue_indication_or_confirmation().first == empty );

        indication_confirmed( 2u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ indication, 16u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( confirming_the_wrong_index, queue8 )
    {
        queue_indication( 7u );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ indication, 7u } ) );

        indication_confirmed( 42u );

        queue_indication( 2u );
        BOOST_CHECK( dequeue_indication_or_confirmation().first == empty );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( clearing )

    BOOST_FIXTURE_TEST_CASE( still_empty, queue8 )
    {
        clear_indications_and_confirmations();
        BOOST_CHECK( dequeue_indication_or_confirmation().first == empty );
    }

    BOOST_FIXTURE_TEST_CASE( clear_all, queue3 )
    {
        queue_notification( 2 );
        queue_indication( 2 );

        clear_indications_and_confirmations();
        BOOST_CHECK( dequeue_indication_or_confirmation().first == empty );
    }

    BOOST_FIXTURE_TEST_CASE( clear_outstanding_confirmation, queue3 )
    {
        queue_indication( 2 );
        BOOST_REQUIRE( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ indication, 2u } ) );

        clear_indications_and_confirmations();

        queue_indication( 2 );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ indication, 2u } ) );
    }

BOOST_AUTO_TEST_SUITE_END()
