#include <iostream>
#include <bluetoe/link_layer/notification_queue.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace {
    struct empty_fixture {};
}

using queue17 = bluetoe::link_layer::notification_queue< std::tuple< std::integral_constant< int, 17u > >, empty_fixture >;
using queue3 = bluetoe::link_layer::notification_queue< std::tuple< std::integral_constant< int, 3u > >, empty_fixture >;
using queue8 = bluetoe::link_layer::notification_queue< std::tuple< std::integral_constant< int, 8u > >, empty_fixture >;

BOOST_AUTO_TEST_SUITE( single_prio_notifications )

    BOOST_FIXTURE_TEST_CASE( empty_queue, queue17 )
    {
        BOOST_CHECK( dequeue_indication_or_confirmation().first == entry_type::empty );
    }

    BOOST_FIXTURE_TEST_CASE( adding_one_notification, queue17 )
    {
        BOOST_CHECK( queue_notification( 12u ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 12u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( dequed_elements_are_removed, queue17 )
    {
        BOOST_CHECK( queue_notification( 12u ) );
        dequeue_indication_or_confirmation();
        BOOST_CHECK( dequeue_indication_or_confirmation().first == entry_type::empty );
    }

    BOOST_FIXTURE_TEST_CASE( adding_two_notification, queue17 )
    {
        BOOST_CHECK( queue_notification( 12u ) );
        BOOST_CHECK( queue_notification( 16u ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 12u } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 16u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( adding_notification_twice, queue17 )
    {
        BOOST_CHECK( queue_notification( 12u ) );
        BOOST_CHECK( !queue_notification( 12u ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 12u } ) );
        BOOST_CHECK( dequeue_indication_or_confirmation().first == entry_type::empty );
    }

    /*
     * The queue should behave a little bit like a queue and thus not always deliver the same element
     */
    BOOST_FIXTURE_TEST_CASE( queue_like, queue17 )
    {
        BOOST_CHECK( queue_notification( 12u ) );
        BOOST_CHECK( queue_notification( 16u ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 12u } ) );

        BOOST_CHECK( queue_notification( 12u ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 16u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( wrap_around, queue17 )
    {
        BOOST_CHECK( queue_notification( 16u ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 16u } ) );

        BOOST_CHECK( queue_notification( 0u ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 0u } ) );

        BOOST_CHECK( dequeue_indication_or_confirmation().first == entry_type::empty );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( single_prio_indications )

    BOOST_FIXTURE_TEST_CASE( queue_an_indication, queue3 )
    {
        BOOST_CHECK( queue_indication( 2 ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 2u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( mixed_notification_and_indications, queue3 )
    {
        BOOST_CHECK( queue_notification( 2 ) );
        BOOST_CHECK( queue_indication( 2 ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 2u } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 2u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( no_indication_until_confirmed, queue17 )
    {
        BOOST_CHECK( queue_notification( 12u ) );
        BOOST_CHECK( queue_indication( 2u ) );
        BOOST_CHECK( queue_indication( 16u ) );

        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 2u } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 12u } ) );
        BOOST_CHECK( dequeue_indication_or_confirmation().first == entry_type::empty );

        indication_confirmed();
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 16u } ) );
    }

    BOOST_FIXTURE_TEST_CASE( double_indication_recognized, queue17 )
    {
        BOOST_CHECK( queue_indication( 16u ) );
        BOOST_CHECK( !queue_indication( 16u ) );

        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 16u } ) );

        BOOST_CHECK( !queue_indication( 16u ) );
        indication_confirmed();

        BOOST_CHECK( queue_indication( 16u ) );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( single_prio_clearing )

    BOOST_FIXTURE_TEST_CASE( still_empty, queue8 )
    {
        clear_indications_and_confirmations();
        BOOST_CHECK( dequeue_indication_or_confirmation().first == entry_type::empty );
    }

    BOOST_FIXTURE_TEST_CASE( clear_all, queue3 )
    {
        BOOST_CHECK( queue_notification( 2 ) );
        BOOST_CHECK( queue_indication( 2 ) );

        clear_indications_and_confirmations();
        BOOST_CHECK( dequeue_indication_or_confirmation().first == entry_type::empty );
    }

    BOOST_FIXTURE_TEST_CASE( clear_outstanding_confirmation, queue3 )
    {
        BOOST_CHECK( queue_indication( 2 ) );
        BOOST_REQUIRE( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 2u } ) );

        clear_indications_and_confirmations();

        BOOST_CHECK( queue_indication( 2 ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 2u } ) );
    }

BOOST_AUTO_TEST_SUITE_END()

using queue1 = bluetoe::link_layer::notification_queue< std::tuple< std::integral_constant< int, 1u > >, empty_fixture >;

BOOST_AUTO_TEST_SUITE( single_prio_single_char_notifications )

    BOOST_FIXTURE_TEST_CASE( not_empty, queue1 )
    {
        BOOST_CHECK( queue_notification( 0 ) );
        BOOST_CHECK( !queue_notification( 0 ) );
    }

    BOOST_FIXTURE_TEST_CASE( dequeue_empty, queue1 )
    {
        BOOST_CHECK( ( dequeue_indication_or_confirmation().first == entry_type::empty ) );
    }

    BOOST_FIXTURE_TEST_CASE( dequeue_not_empty, queue1 )
    {
        queue_notification( 0 );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 0 } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation().first == entry_type::empty ) );
        BOOST_CHECK( queue_notification( 0 ) );
    }

    BOOST_FIXTURE_TEST_CASE( reset, queue1 )
    {
        BOOST_CHECK( queue_notification( 0 ) );
        clear_indications_and_confirmations();
        BOOST_CHECK( queue_notification( 0 ) );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( single_prio_single_char_indications )

    BOOST_FIXTURE_TEST_CASE( no_empty, queue1 )
    {
        BOOST_CHECK( queue_indication( 0 ) );
        BOOST_CHECK( !queue_indication( 0 ) );
    }

    BOOST_FIXTURE_TEST_CASE( dequeue_not_empty, queue1 )
    {
        queue_indication( 0 );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 0 } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation().first == entry_type::empty ) );
        BOOST_CHECK( !queue_indication( 0 ) );
    }

    BOOST_FIXTURE_TEST_CASE( confirm_indication, queue1 )
    {
        queue_indication( 0 );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 0 } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation().first == entry_type::empty ) );
        BOOST_CHECK( !queue_indication( 0 ) );

        indication_confirmed();
        BOOST_CHECK( queue_indication( 0 ) );
    }

    BOOST_FIXTURE_TEST_CASE( reset, queue1 )
    {
        BOOST_CHECK( queue_indication( 0 ) );
        clear_indications_and_confirmations();
        BOOST_CHECK( queue_indication( 0 ) );
    }

BOOST_AUTO_TEST_SUITE_END()

using queue1_2 = bluetoe::link_layer::notification_queue<
    std::tuple<
        std::integral_constant< int, 1u >,
        std::integral_constant< int, 2u >
    >, empty_fixture >;

BOOST_AUTO_TEST_SUITE( mixed_priorities )

    BOOST_FIXTURE_TEST_CASE( queue_empty, queue1_2 )
    {
        BOOST_CHECK( ( dequeue_indication_or_confirmation().first == entry_type::empty ) );
    }

    BOOST_FIXTURE_TEST_CASE( higher_prio, queue1_2 )
    {
        BOOST_CHECK( queue_notification( 2 ) );
        BOOST_CHECK( !queue_notification( 2 ) );
        BOOST_CHECK( queue_notification( 1 ) );
        BOOST_CHECK( !queue_notification( 1 ) );
        BOOST_CHECK( queue_notification( 0 ) );
        BOOST_CHECK( !queue_notification( 0 ) );

        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 0 } ) );
        BOOST_CHECK( queue_notification( 0 ) );

        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 0 } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 1 } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 2 } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation().first == entry_type::empty ) );
    }

    BOOST_FIXTURE_TEST_CASE( outstanding_indication_blocks_new_indiciation, queue1_2 )
    {
        BOOST_CHECK( queue_notification( 2 ) );
        BOOST_CHECK( !queue_notification( 2 ) );
        BOOST_CHECK( queue_indication( 1 ) );
        BOOST_CHECK( !queue_indication( 1 ) );

        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::indication, 1 } ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation()  == std::pair< entry_type, std::size_t >{ entry_type::notification, 2 } ) );

        BOOST_CHECK( queue_indication( 0 ) );
        BOOST_CHECK( ( dequeue_indication_or_confirmation().first == entry_type::empty ) );
    }

    // there was a bug in the implementation, where a class derived from all the elements
    using queue1_1_2 = bluetoe::link_layer::notification_queue<
    std::tuple<
        std::integral_constant< int, 1u >,
        std::integral_constant< int, 1u >,
        std::integral_constant< int, 2u >
    >, empty_fixture >;

    BOOST_FIXTURE_TEST_CASE( implementation_without_ambiguous_base_classes, queue1_1_2 )
    {
        // just make sure, that this case compiles
    }

BOOST_AUTO_TEST_SUITE_END()
