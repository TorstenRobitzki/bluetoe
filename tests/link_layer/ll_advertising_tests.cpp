#include "buffer_io.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <bluetoe/link_layer.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/custom_advertising.hpp>
#include <bluetoe/server.hpp>
#include "test_radio.hpp"
#include "connected.hpp"
#include "test_servers.hpp"

#include <map>

namespace {

    template < typename ... Options >
    struct advertising_base : bluetoe::link_layer::link_layer< test::small_temperature_service, test::radio, Options... >
    {
        advertising_base()
        {
            this->run( gatt_server_ );
        }

        test::small_temperature_service gatt_server_;
    };

    struct advertising : advertising_base< test::buffer_sizes > {};

    template < typename ... Options >
    struct advertising_and_connect_base : bluetoe::link_layer::link_layer< test::small_temperature_service, test::radio, Options... >
    {
        typedef bluetoe::link_layer::link_layer< test::small_temperature_service, test::radio, Options... > base;

        void run()
        {
            test::small_temperature_service gatt_server_;
            base::run( gatt_server_ );
        }

        /*
         * this function runs the simulation and checks that no response was generated
         */
        void run_expect_no_response( const char* message )
        {
            run();

            this->check_scheduling(
                []( const test::advertising_data& pdu ) -> bool
                {
                    return ( pdu.transmitted_data[ 0 ] & 0xf ) == 0;
                },
                message
            );
        }


    };

    struct advertising_and_connect : advertising_and_connect_base< test::buffer_sizes > {};

    static const auto filter_channel_37 = []( const test::advertising_data& a )
    {
        return a.channel == 37;
    };

}

BOOST_FIXTURE_TEST_CASE( advertising_scheduled, advertising )
{
    BOOST_CHECK_GT( advertisings().size(), 0u );
}

BOOST_FIXTURE_TEST_CASE( advertising_uses_all_three_adv_channels, advertising )
{
    std::map< unsigned, unsigned > channel;

    all_data( [&]( const test::advertising_data& d ) { ++channel[ d.channel ]; } );

    BOOST_CHECK_EQUAL( channel.size(), 3u );
    BOOST_CHECK_GT( channel[ 37 ], 0u );
    BOOST_CHECK_GT( channel[ 38 ], 0u );
    BOOST_CHECK_GT( channel[ 39 ], 0u );
}

BOOST_FIXTURE_TEST_CASE( channels_are_iterated, advertising )
{
    check_scheduling(
        [&]( const test::advertising_data& a, const test::advertising_data& b )
        {
            return a.channel != b.channel;
        },
        "channels_are_iterated"
    );
}

/**
 * @test by default, the link layer will advertise Connectable Undirected Events
 */
BOOST_FIXTURE_TEST_CASE( connectable_undirected_is_the_default, advertising )
{
    check_scheduling(
        [&]( const test::advertising_data& scheduled ) -> bool
        {
            return !scheduled.transmitted_data.empty()
                && ( scheduled.transmitted_data[ 0 ] & 0xf ) == 0;
        },
        "connectable_undirected_is_the_default"
    );
}

/**
 * @test according to the specs, the distance between two advertising PDUs shall be shorter than or equal to 10ms
 *       We identify the start of an advertising event by the first channel 37
 */
BOOST_FIXTURE_TEST_CASE( less_than_10ms_between_two_PDUs, advertising )
{
    check_scheduling(
        [&]( const test::advertising_data& a, const test::advertising_data& b )
        {
            return ( a.channel != 37 && a.channel != 38 )
                || ( b.on_air_time - a.on_air_time ) <= bluetoe::link_layer::delta_time::msec( 10 );
        },
        "less_than_10ms_between_two_PDUs"
    );
}

BOOST_FIXTURE_TEST_CASE( correct_access_address_is_used, advertising )
{
    check_scheduling(
        []( const test::advertising_data& data )
        {
            return data.access_address == 0x8E89BED6;
        },
        "correct_access_address_is_used"
    );
}

BOOST_FIXTURE_TEST_CASE( correct_crc_init_is_used, advertising )
{
    check_scheduling(
        []( const test::advertising_data& data )
        {
            return data.crc_init == 0x555555;
        },
        "correct_crc_init_is_used"
    );
}

/**
 * @test the advertising interval is 50ms plus a pseudo random interval of 0ms to 10ms
 */
using interval_50_ms = advertising_base< bluetoe::link_layer::advertising_interval< 50 >, test::buffer_sizes >;
BOOST_FIXTURE_TEST_CASE( configured_advertising_interval_is_kept, interval_50_ms )
{
    check_scheduling(
        filter_channel_37,
        [&]( const test::advertising_data& a, const test::advertising_data& b )
        {
            const auto diff = b.on_air_time - a.on_air_time;

            return diff >= bluetoe::link_layer::delta_time::msec( 50 )
                && diff <= bluetoe::link_layer::delta_time::msec( 60 );
        },
        "configured_advertising_interval_is_kept"
    );
}

/**
 * @test by default, the interval is 100ms
 */
BOOST_FIXTURE_TEST_CASE( default_advertising_interval_is_kept, advertising )
{
    check_scheduling(
        filter_channel_37,
        [&]( const test::advertising_data& a, const test::advertising_data& b )
        {
            const auto diff = b.on_air_time - a.on_air_time;

            return diff >= bluetoe::link_layer::delta_time::msec( 100 )
                && diff <= bluetoe::link_layer::delta_time::msec( 110 );
        },
        "configured_advertising_interval_is_kept"
    );
}

/**
 * @test perturbation looks quit random
 * - no two adjanced equal values
 * - all values are equally distributed.
 */
BOOST_FIXTURE_TEST_CASE( perturbation_looks_quit_random, advertising )
{
    std::map< bluetoe::link_layer::delta_time, unsigned >   perturbations;
    bluetoe::link_layer::delta_time                         last_perturbation = bluetoe::link_layer::delta_time::now();

    all_data(
        filter_channel_37,
        [&]( const test::advertising_data& a, const test::advertising_data& b )
        {
            const auto current_perturbation = b.on_air_time - a.on_air_time - bluetoe::link_layer::delta_time::msec( 100 );

            if ( !perturbations.empty() )
            {
                BOOST_CHECK_NE( last_perturbation, current_perturbation );
            }

            ++perturbations[ current_perturbation ];
            last_perturbation = current_perturbation;
        }
    );

    double average = 0.0;
    unsigned count = 0;

    for ( auto p : perturbations )
    {
        average += p.first.usec() * p.second;
        count   += p.second;
    }

    BOOST_CHECK_CLOSE_FRACTION( ( average / count ), 5.0 * 1000, 0.2 );
}

BOOST_FIXTURE_TEST_CASE( sending_advertising_pdus, advertising )
{
    check_scheduling(
        []( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 2 && ( pdu[ 0 ] & 0x0f ) == 0;
        },
        "sending_advertising_pdus"
    );
}

BOOST_FIXTURE_TEST_CASE( advertising_pdus_contain_the_address, advertising )
{
    static const bluetoe::link_layer::address expected_address = bluetoe::link_layer::address::generate_static_random_address( 0x47110815 );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;
            return pdu.size() >= 8 && bluetoe::link_layer::address( &pdu[ 2 ] ) == expected_address;
        },
        "advertising_pdus_contain_the_address"
    );
}

BOOST_FIXTURE_TEST_CASE( txadd_and_rxadd_bits_are_set_corretly_for_random_address, advertising )
{
    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;
            return pdu.size() >= 1 && ( pdu[ 0 ] & ( 3 << 6 ) ) == ( 1 << 6 );
        },
        "txadd_and_rxadd_bits_are_set_corretly_for_random_address"
    );
}

BOOST_FIXTURE_TEST_CASE( length_field_is_set_corretly, advertising )
{
    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;
            return pdu.size() >= 2 && ( pdu[ 1 ] & 0x3f ) == pdu.size() - 2;
        },
        "length_field_is_set_corretly"
    );
}

BOOST_FIXTURE_TEST_CASE( pdus_contain_the_gap_data, advertising )
{
    std::uint8_t        gap[ 31 ];
    const std::size_t   gap_size = gatt_server_.advertising_data( &gap[ 0 ], sizeof( gap ) );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;
            return pdu.size() == 8 + gap_size && std::equal( &pdu[ 8 ], &pdu[ 8 + gap_size ], &gap[ 0 ] );
        },
        "pdus_contain_the_gap_data"
    );
}

BOOST_FIXTURE_TEST_CASE( no_repond_to_an_invalid_pdu, advertising_and_connect )
{
    respond_to(
        37, // channel
        {
            0x03, 0x0C // just a header
        }
    );

    run_expect_no_response( "no_repond_to_an_invalid_pdu" );
}

/**
 * @test no respond if the device is not addressed
 */
BOOST_FIXTURE_TEST_CASE( no_repond_to_a_scan_request_with_different_address, advertising_and_connect )
{
    respond_to(
        37, // channel
        {
            0x03, 0x0C, // header
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // scanner address
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc1  // not advertiser address
        }
    );

    run_expect_no_response( "no_repond_to_a_scan_request_with_different_address" );
}

BOOST_FIXTURE_TEST_CASE( still_advertising_after_an_invalid_pdu, advertising_and_connect )
{
    respond_to(
        37, // channel
        {
            0x03, 0x06, // header
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06  // scanner address
            // missing advertiser address
        }
    );

    run();

    const unsigned number_of_advertising_packages =
        this->sum_data(
            std::function< unsigned ( const test::advertising_data&, unsigned ) >(
                []( const test::advertising_data& pdu, unsigned start_value )
                {
                    return ( pdu.transmitted_data[ 0 ] & 0xf ) == 0
                        ? start_value + 1
                        : start_value;
                }
            ),
            0u
        );

    BOOST_CHECK_GT( number_of_advertising_packages, 5u );
}

BOOST_FIXTURE_TEST_CASE( after_beeing_connected_the_ll_starts_to_advertise_again, connected_and_timeout )
{
    BOOST_REQUIRE( !advertisings().empty() );
    auto const adv = advertisings().back();

    BOOST_REQUIRE( !connection_events().empty() );
    auto const con = connection_events().back();

    BOOST_CHECK_GT( adv.schedule_time, con.schedule_time );
}

BOOST_FIXTURE_TEST_CASE( after_advertising_again_the_right_crc_have_to_be_used, connected_and_timeout )
{
    auto const adv = advertisings().back();

    BOOST_CHECK_EQUAL( adv.crc_init, 0x555555u );
}

BOOST_FIXTURE_TEST_CASE( after_advertising_again_the_right_access_address_have_to_be_used, connected_and_timeout )
{
    auto const adv = advertisings().back();

    BOOST_CHECK_EQUAL( adv.access_address, 0x8E89BED6u );
}

using advertising_connectable_undirected_advertising  =
    advertising_base<
        bluetoe::link_layer::connectable_undirected_advertising,
        test::buffer_sizes
    >;

BOOST_FIXTURE_TEST_CASE( starting_advertising_connectable_undirected_advertising, advertising_connectable_undirected_advertising )
{
    const auto number_of_advertising_pdus =
        count_data( []( const test::advertising_data& scheduled ) -> bool
        {
            return !scheduled.transmitted_data.empty()
                && ( scheduled.transmitted_data[ 0 ] & 0xf ) == 0;
        } );

    BOOST_CHECK_GT( number_of_advertising_pdus, 0u );
}

BOOST_AUTO_TEST_SUITE( connectable_directed_advertising )

using advertising_connectable_directed_advertising  =
    advertising_base<
        bluetoe::link_layer::connectable_directed_advertising,
        test::buffer_sizes
    >;

// an address is needed to start the advertising
BOOST_FIXTURE_TEST_CASE( directed_advertising_doesnt_starts_by_default, advertising_connectable_directed_advertising )
{
    BOOST_CHECK_EQUAL( 0u, count_data( []( const test::advertising_data& ) -> bool {
        return true;
    } ) );
}

//
BOOST_FIXTURE_TEST_CASE( starting_connectable_directed_advertising, advertising_connectable_directed_advertising )
{
    directed_advertising_address( bluetoe::link_layer::public_device_address( { 0x00, 0x00, 0x00, 0x01, 0x0f, 0xc0 } ) );

    run( gatt_server_ );
    BOOST_CHECK_GT( advertisings().size(), 0u );
}

struct started_directed_advertising : bluetoe::link_layer::link_layer< test::small_temperature_service, test::radio, bluetoe::link_layer::connectable_directed_advertising >
{
    started_directed_advertising()
    {
        directed_advertising_address( bluetoe::link_layer::public_device_address( { 0x00, 0x00, 0x00, 0x01, 0x0f, 0xc0 } ) );
        this->run( gatt_server_ );
    }

    test::small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( stop_connectable_directed_advertising, started_directed_advertising )
{
    directed_advertising_address( bluetoe::link_layer::device_address() );

    run( gatt_server_ );
    clear_events();

    BOOST_CHECK_EQUAL( 0u, count_data( []( const test::advertising_data& ) -> bool {
        return true;
    } ) );
}

BOOST_FIXTURE_TEST_CASE( correct_size_and_type, started_directed_advertising )
{
    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() == 2 * 6 + 2
                && ( pdu[ 0 ] & 0xf ) == 1
                && ( pdu[ 1 ] & 0x3f ) == 2 * 6;
        },
        "correct_size_and_type"
    );
}

BOOST_FIXTURE_TEST_CASE( contains_local_address, started_directed_advertising )
{
    const auto address = local_address();

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 8
                && std::equal( &pdu[ 2 ], &pdu[ 8 ], address.begin() )
                && ( pdu[ 0 ] & 0x40 ) != 0;
        },
        "contains_local_address"
    );
}

BOOST_FIXTURE_TEST_CASE( contains_remote_address, started_directed_advertising )
{
    const auto address = bluetoe::link_layer::public_device_address( { 0x00, 0x00, 0x00, 0x01, 0x0f, 0xc0 } );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 14
                && std::equal( &pdu[ 8 ], &pdu[ 14 ], address.begin() )
                && ( pdu[ 0 ] & 0x80 ) == 0;
        },
        "contains_remote_address"
    );
}

BOOST_FIXTURE_TEST_CASE( is_connectable_from_directed_address, started_directed_advertising )
{
    BOOST_REQUIRE( connection_events().empty() );

    respond_to(
        37,
        {
            0x85, 0x22,                         // header
            0x00, 0x00, 0x00, 0x01, 0x0f, 0xc0, // InitA: c0:0f:01:00:00:00 (public)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    end_of_simulation( bluetoe::link_layer::delta_time::seconds( 20 ) );
    run( gatt_server_ );

    BOOST_CHECK( !connection_events().empty() );
}

BOOST_FIXTURE_TEST_CASE( is_not_connectable_from_other_addresses, started_directed_advertising )
{
    BOOST_REQUIRE( connection_events().empty() );

    respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    end_of_simulation( bluetoe::link_layer::delta_time::seconds( 20 ) );
    run( gatt_server_ );

    BOOST_CHECK( connection_events().empty() );
}

BOOST_FIXTURE_TEST_CASE( no_response_to_scan_request, started_directed_advertising )
{
    respond_to(
        37, // channel
        {
            0xC3, 0x0C, // header
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // scanner address
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0  // advertiser address
        }
    );

    end_of_simulation( bluetoe::link_layer::delta_time::seconds( 20 ) );
    run( gatt_server_ );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 1
                && ( pdu[ 0 ] & 0xf ) == 1;
        },
        "no_response_to_scan_request"
    );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( non_connectable_undirected_advertising )

struct non_connectable_undirected_advertising :
    bluetoe::link_layer::link_layer< test::small_temperature_service, test::radio, bluetoe::link_layer::non_connectable_undirected_advertising, test::buffer_sizes >
{
    test::small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( starting_advertising, non_connectable_undirected_advertising )
{
    this->run( gatt_server_ );

    BOOST_CHECK_GT( advertisings().size(), 0u );
}

BOOST_FIXTURE_TEST_CASE( correct_type_and_size, non_connectable_undirected_advertising )
{
    this->run( gatt_server_ );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 2
                && ( pdu[ 0 ] & 0xf ) == 2
                && ( pdu[ 1 ] & 0x3f ) == pdu.size() - 2;
        },
        "correct_type_and_size"
    );
}

BOOST_FIXTURE_TEST_CASE( contains_local_address, non_connectable_undirected_advertising )
{
    this->run( gatt_server_ );
    const auto address = local_address();

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 8
                && std::equal( &pdu[ 2 ], &pdu[ 8 ], address.begin() )
                && ( pdu[ 0 ] & 0x40 ) != 0;
        },
        "contains_local_address"
    );
}

BOOST_FIXTURE_TEST_CASE( no_response_to_connection_request_request, non_connectable_undirected_advertising )
{
    respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x00, 0x00, 0x00, 0x01, 0x0f, 0xc0, // InitA: c0:0f:01:00:00:00 (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    run( gatt_server_ );

    BOOST_CHECK( connection_events().empty() );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( scannable_undirected_advertising )

struct scannable_undirected_advertising :
    bluetoe::link_layer::link_layer< test::small_temperature_service, test::radio, bluetoe::link_layer::scannable_undirected_advertising, test::buffer_sizes >
{
    test::small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( starting_advertising, scannable_undirected_advertising )
{
    this->run( gatt_server_ );

    BOOST_CHECK_GT( advertisings().size(), 0u );
}

BOOST_FIXTURE_TEST_CASE( correct_type_and_size, scannable_undirected_advertising )
{
    this->run( gatt_server_ );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 2
                && ( pdu[ 0 ] & 0xf ) == 6
                && ( pdu[ 1 ] & 0x3f ) == pdu.size() - 2;
        },
        "correct_type_and_size"
    );
}

BOOST_FIXTURE_TEST_CASE( contains_local_address, scannable_undirected_advertising )
{
    this->run( gatt_server_ );
    const auto address = local_address();

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 8
                && std::equal( &pdu[ 2 ], &pdu[ 8 ], address.begin() )
                && ( pdu[ 0 ] & 0x40 ) != 0;
        },
        "contains_local_address"
    );
}

BOOST_FIXTURE_TEST_CASE( no_response_to_connection_request_request, scannable_undirected_advertising )
{
    respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x00, 0x00, 0x00, 0x01, 0x0f, 0xc0, // InitA: c0:0f:01:00:00:00 (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    run( gatt_server_ );

    BOOST_CHECK( connection_events().empty() );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( multiple_advertising_types )

struct scannable_and_connectable_advertising :
    bluetoe::link_layer::link_layer<
        test::small_temperature_service, test::radio,
        bluetoe::link_layer::connectable_undirected_advertising,
        bluetoe::link_layer::scannable_undirected_advertising,
        test::buffer_sizes
    >
{
    test::small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( advertising_starts_with_the_first_named_type, scannable_and_connectable_advertising )
{
    run( gatt_server_ );

    BOOST_REQUIRE( !advertisings().empty() );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 1
                && ( pdu[ 0 ] & 0xf ) == 0;
        },
        "advertising_starts_with_the_first_names_type"
    );
}

BOOST_FIXTURE_TEST_CASE( advertising_is_repeated_after_timeout, scannable_and_connectable_advertising )
{
    run( gatt_server_ );

    const auto number_of_advertising_pdus =
        count_data( []( const test::advertising_data& scheduled ) -> bool
        {
            return !scheduled.transmitted_data.empty()
                && ( scheduled.transmitted_data[ 0 ] & 0xf ) == 0;
        } );

    BOOST_CHECK_GT( number_of_advertising_pdus, 1u );
}

BOOST_FIXTURE_TEST_CASE( can_switch_type_before_starting, scannable_and_connectable_advertising )
{
    this->change_advertising< bluetoe::link_layer::scannable_undirected_advertising >();

    run( gatt_server_ );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 1
                && ( pdu[ 0 ] & 0xf ) == 6;
        },
        "advertising_starts_with_the_first_names_type"
    );
}

BOOST_FIXTURE_TEST_CASE( can_not_switch_type_after_starting, scannable_and_connectable_advertising )
{
    run( gatt_server_ );

    this->change_advertising< bluetoe::link_layer::scannable_undirected_advertising >();

    run( gatt_server_ );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 1
                && ( pdu[ 0 ] & 0xf ) == 0;
        },
        "can_not_switch_type_after_starting"
    );
}

BOOST_FIXTURE_TEST_CASE( switching_type_is_defered_until_the_next_adv_start, scannable_and_connectable_advertising )
{
    run( gatt_server_ );

    this->change_advertising< bluetoe::link_layer::scannable_undirected_advertising >();

    respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x00, 0x00, 0x00, 0x01, 0x0f, 0xc0, // InitA: c0:0f:01:00:00:00 (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    end_of_simulation( bluetoe::link_layer::delta_time::seconds( 20 ) );
    run( gatt_server_ );

    BOOST_REQUIRE( !connection_events().empty() );

    BOOST_CHECK_GT( count_data(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 1
                && ( pdu[ 0 ] & 0xf ) == 6;
        }), 0u );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( variable_advertising_interval )

struct variable_interval_multiple_types :
    bluetoe::link_layer::link_layer<
        test::small_temperature_service, test::radio,
        bluetoe::link_layer::connectable_undirected_advertising,
        bluetoe::link_layer::scannable_undirected_advertising,
        bluetoe::link_layer::variable_advertising_interval,
        test::buffer_sizes
    >
{
    test::small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( the_default_is_100ms, variable_interval_multiple_types )
{
    this->run( gatt_server_ );

    check_scheduling(
        filter_channel_37,
        [&]( const test::advertising_data& a, const test::advertising_data& b )
        {
            const auto diff = b.on_air_time - a.on_air_time;

            return diff >= bluetoe::link_layer::delta_time::msec( 100 )
                && diff <= bluetoe::link_layer::delta_time::msec( 110 );
        },
        "the_default_is_100ms"
    );
}

BOOST_FIXTURE_TEST_CASE( changeable_to_30, variable_interval_multiple_types )
{
    advertising_interval_ms( 30 );
    run( gatt_server_ );

    check_scheduling(
        filter_channel_37,
        [&]( const test::advertising_data& a, const test::advertising_data& b )
        {
            const auto diff = b.on_air_time - a.on_air_time;

            return diff >= bluetoe::link_layer::delta_time::msec( 30 )
                && diff <= bluetoe::link_layer::delta_time::msec( 40 );
        },
        "changeable_to_300"
    );
}

struct variable_interval_single_type :
    bluetoe::link_layer::link_layer<
        test::small_temperature_service, test::radio,
        bluetoe::link_layer::variable_advertising_interval,
        test::buffer_sizes
    >
{
    test::small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( works_for_single_types_too, variable_interval_single_type )
{
    advertising_interval_ms( 30 );
    run( gatt_server_ );

    check_scheduling(
        filter_channel_37,
        [&]( const test::advertising_data& a, const test::advertising_data& b )
        {
            const auto diff = b.on_air_time - a.on_air_time;

            return diff >= bluetoe::link_layer::delta_time::msec( 30 )
                && diff <= bluetoe::link_layer::delta_time::msec( 40 );
        },
        "changeable_to_300"
    );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( no_auto_start_advertising )

struct no_auto_start_single_type :
    bluetoe::link_layer::link_layer<
        test::small_temperature_service, test::radio,
        bluetoe::link_layer::no_auto_start_advertising,
        test::buffer_sizes
    >
{
    test::small_temperature_service gatt_server_;
};

struct no_auto_start_multiple_type :
    bluetoe::link_layer::link_layer<
        test::small_temperature_service, test::radio,
        bluetoe::link_layer::no_auto_start_advertising,
        bluetoe::link_layer::connectable_undirected_advertising,
        bluetoe::link_layer::scannable_undirected_advertising,
        test::buffer_sizes
    >
{
    test::small_temperature_service gatt_server_;
};

typedef boost::mpl::list<
    no_auto_start_single_type,
    no_auto_start_multiple_type > all_fixtures;

BOOST_AUTO_TEST_CASE_TEMPLATE( no_automatic_start_single, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.run( link_layer.gatt_server_ );
    BOOST_CHECK_EQUAL( 0u, link_layer.advertisings().size() );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( manuell_start_before, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.start_advertising();
    link_layer.run( link_layer.gatt_server_ );

    BOOST_CHECK_NE( 0u, link_layer.advertisings().size() );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( manuell_start_single_after, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.run( link_layer.gatt_server_ );

    link_layer.start_advertising();

    link_layer.end_of_simulation( bluetoe::link_layer::delta_time::seconds( 20 ) );
    link_layer.run( link_layer.gatt_server_ );

    BOOST_CHECK_NE( 0u, link_layer.advertisings().size() );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( no_automatic_start_after_disconnect, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.start_advertising();
    link_layer.respond_to(
        37,
        {
            0xc5, 0x22,                         // header
            0x00, 0x00, 0x00, 0x01, 0x0f, 0xc0, // InitA: c0:0f:01:00:00:00 (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            0x03,                               // transmit window size
            0x0b, 0x00,                         // window offset
            0x18, 0x00,                         // interval
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        }
    );

    link_layer.run( link_layer.gatt_server_ );

    BOOST_REQUIRE( !link_layer.connection_events().empty() );
    const auto connect_time = link_layer.connection_events().front().schedule_time;

    link_layer.check_scheduling(
        [&]( const test::advertising_data& adv )
        {
            return adv.schedule_time <= connect_time;
        },
        "no_automatic_start_after_disconnect"
    );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stop_advertising_test, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.start_advertising();
    link_layer.run( link_layer.gatt_server_ );

    const auto adv_count = link_layer.advertisings().size();
    link_layer.stop_advertising();

    link_layer.end_of_simulation( bluetoe::link_layer::delta_time::seconds( 20 ) );
    link_layer.run( link_layer.gatt_server_ );

    BOOST_CHECK_EQUAL( link_layer.advertisings().size(), adv_count );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stop_advertising_after_one_advertisements, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.start_advertising( 1u );
    link_layer.run( link_layer.gatt_server_ );

    BOOST_CHECK_EQUAL( 1u, link_layer.advertisings().size() );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stop_advertising_after_certain_advertisements, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.start_advertising( 42u );
    link_layer.run( link_layer.gatt_server_ );

    BOOST_CHECK_EQUAL( 42u, link_layer.advertisings().size() );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( restart_after_count_down, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.start_advertising( 42u );
    link_layer.run( link_layer.gatt_server_ );

    BOOST_CHECK_EQUAL( 42u, link_layer.advertisings().size() );

    link_layer.start_advertising();
    link_layer.end_of_simulation( bluetoe::link_layer::delta_time::seconds( 20 ) );
    link_layer.run( link_layer.gatt_server_ );

    BOOST_CHECK_GT( link_layer.advertisings().size(), 42u );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( stop_count_down, LinkLayer, all_fixtures )
{
    LinkLayer link_layer;
    link_layer.start_advertising( 300 );
    link_layer.run( link_layer.gatt_server_ );

    // 290 is simply the number of advertising PDUs send within 10 second
    BOOST_REQUIRE_EQUAL( 290u, link_layer.advertisings().size() );

    link_layer.start_advertising();

    link_layer.end_of_simulation( bluetoe::link_layer::delta_time::seconds( 20 ) );
    link_layer.run( link_layer.gatt_server_ );

    BOOST_CHECK_GT( link_layer.advertisings().size(), 550u );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( advertising_custom_data )

    static const std::uint8_t custom_advertising_data[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
    };

    static const std::uint8_t custom_response_data[] = {
        0x11, 0x22, 0x33, 0x44, 0x55
    };

    using server = bluetoe::extend_server<
        test::small_temperature_service,
        bluetoe::custom_advertising_data< sizeof( custom_advertising_data ), custom_advertising_data >,
        bluetoe::custom_scan_response_data< sizeof( custom_response_data ), custom_response_data >
    >;

    template < typename ... Options >
    struct custom_advertising : bluetoe::link_layer::link_layer< server, test::radio, Options... >
    {
        custom_advertising()
        {
            this->run( gatt_server_ );
        }

        server gatt_server_;
    };

    BOOST_FIXTURE_TEST_CASE( advertisng_data, custom_advertising<> )
    {
        run( gatt_server_ );

        const std::uint8_t expected[] = {
            0x40, 0x0d,                         // Header
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // data
            0x07
        };

        check_scheduling(
            [&]( const test::advertising_data& data )
            {
                const auto& pdu = data.transmitted_data;

                return pdu.size() == sizeof( expected )
                    && std::equal( pdu.begin(), pdu.end(), std::begin( expected ) );
            },
            "advertsing data"
        );
    }

BOOST_AUTO_TEST_SUITE_END()
