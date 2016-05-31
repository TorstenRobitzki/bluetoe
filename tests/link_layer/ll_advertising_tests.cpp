#include "buffer_io.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/server.hpp>
#include "test_radio.hpp"
#include "connected.hpp"
#include "../test_servers.hpp"

#include <map>

namespace {

    template < typename ... Options >
    struct advertising_base : bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... >
    {
        advertising_base()
        {
            this->run( gatt_server_ );
        }

        small_temperature_service gatt_server_;
    };

    struct advertising : advertising_base<> {};

    template < typename ... Options >
    struct advertising_and_connect_base : bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... >
    {
        typedef bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... > base;

        void run()
        {
            small_temperature_service gatt_server_;
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

    struct advertising_and_connect : advertising_and_connect_base<> {};

    static const auto filter_channel_37 = []( const test::advertising_data& a )
    {
        return a.channel == 37;
    };

}

BOOST_FIXTURE_TEST_CASE( advertising_scheduled, advertising )
{
    BOOST_CHECK_GT( advertisings().size(), 0 );
}

BOOST_FIXTURE_TEST_CASE( advertising_uses_all_three_adv_channels, advertising )
{
    std::map< unsigned, unsigned > channel;

    all_data( [&]( const test::advertising_data& d ) { ++channel[ d.channel ]; } );

    BOOST_CHECK_EQUAL( channel.size(), 3u );
    BOOST_CHECK_GT( channel[ 37 ], 0 );
    BOOST_CHECK_GT( channel[ 38 ], 0 );
    BOOST_CHECK_GT( channel[ 39 ], 0 );
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
            return a.channel != 37 && a.channel != 38
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
BOOST_FIXTURE_TEST_CASE( configured_advertising_interval_is_kept, advertising_base< bluetoe::link_layer::advertising_interval< 50 > > )
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

/**
 * @test until now, the link layer should respond with an empty response
 */
BOOST_FIXTURE_TEST_CASE( empty_reponds_to_a_scan_request, advertising_and_connect )
{
    respond_to(
        37, // channel
        {
            0xC3, 0x0C, // header
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // scanner address
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0  // advertiser address
        }
    );

    run();

    static const std::vector< std::uint8_t > expected_response =
    {
        0x44, 0x06,
        0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0
    };

    find_scheduling(
        []( const test::advertising_data& pdu ) -> bool
        {
            return pdu.channel == 37
                && pdu.transmision_time.zero()
                && pdu.transmitted_data == expected_response
                && pdu.receive_buffer.empty();
        },
        "empty_reponds_to_a_scan_request"
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
            0x03, 0x0C, // header
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

    BOOST_CHECK_GT( number_of_advertising_packages, 5 );
}

/**
 * @brief After the SCAN_RSP PDU is sent, the advertiser shall move to the next used advertising channel index to send another ADV_IND PDU
 */
BOOST_FIXTURE_TEST_CASE( move_to_next_chanel_after_adverting, advertising_and_connect )
{
    static const std::initializer_list< std::uint8_t > valid_scan_request = {
        0xC3, 0x0C, // header
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // scanner address
        0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0  // advertiser address
    };

    // simulate 4 scan requests on 4 different channels
    for ( unsigned int c = 0; c != 4; ++c )
    {
        add_responder(
            [c]( const test::advertising_data& d ) -> std::pair< bool, test::advertising_response >
            {
                const unsigned channel = 37 + c % 3;

                return ( d.transmitted_data[ 0 ] & 0xf ) == 0 && d.channel == channel
                    ? std::pair< bool, test::advertising_response >(
                        true,
                        test::advertising_response{ channel, valid_scan_request, T_IFS } )
                    : std::pair< bool, test::advertising_response >( false, test::advertising_response() );
            }
        );
    }

    run();

    unsigned count = 0;

    check_scheduling(
        [ &count ]( const test::advertising_data& first, const test::advertising_data& next ) -> bool
        {
            if ( ( first.transmitted_data[ 0 ] & 0xf ) != 0x4 )
                return true;

            ++count;

            // `first` is an scan response, `second` should now be an advertising message on the next channel
            return ( next.transmitted_data[ 0 ] & 0xf ) == 0
                 && next.channel != first.channel
                 // when changing to the second and third advertising channel, no delay should applied
                 && ( next.transmision_time.zero() || next.channel == 37 );
        },
        "move_to_next_chanel_after_adverting"
    );

    // make sure, the test realy found 4 scan responses
    BOOST_REQUIRE_EQUAL( count, 4 );
}

BOOST_FIXTURE_TEST_CASE( after_beeing_connected_the_ll_starts_to_advertise_again, connected_and_timeout )
{
    auto const adv = advertisings().back();
    auto const con = connection_events().back();

    BOOST_CHECK_GT( adv.schedule_time, con.schedule_time );
}

BOOST_FIXTURE_TEST_CASE( after_advertising_again_the_right_crc_have_to_be_used, connected_and_timeout )
{
    auto const adv = advertisings().back();

    BOOST_CHECK_EQUAL( adv.crc_init, 0x555555 );
}

BOOST_FIXTURE_TEST_CASE( after_advertising_again_the_right_access_address_have_to_be_used, connected_and_timeout )
{
    auto const adv = advertisings().back();

    BOOST_CHECK_EQUAL( adv.access_address, 0x8E89BED6 );
}

using advertising_connectable_undirected_advertising  =
    advertising_base<
        bluetoe::link_layer::connectable_undirected_advertising
    >;

BOOST_FIXTURE_TEST_CASE( starting_advertising_connectable_undirected_advertising, advertising_connectable_undirected_advertising )
{
    const auto number_of_advertising_pdus =
        count_data( []( const test::advertising_data& scheduled ) -> bool
        {
            return !scheduled.transmitted_data.empty()
                && ( scheduled.transmitted_data[ 0 ] & 0xf ) == 0;
        } );

    BOOST_CHECK_GT( number_of_advertising_pdus, 0 );
}

BOOST_AUTO_TEST_SUITE( connectable_directed_advertising )

using advertising_connectable_directed_advertising  =
    advertising_base<
        bluetoe::link_layer::connectable_directed_advertising
    >;

// an address is needed to start the advertising
BOOST_FIXTURE_TEST_CASE( directed_advertising_doesnt_starts_by_default, advertising_connectable_directed_advertising )
{
    BOOST_CHECK_EQUAL( 0, count_data( []( const test::advertising_data& ) -> bool {
        return true;
    } ) );
}

//
BOOST_FIXTURE_TEST_CASE( starting_connectable_directed_advertising, advertising_connectable_directed_advertising )
{
    directed_advertising_address( bluetoe::link_layer::address::generate_static_random_address( 1 ) );

    run( gatt_server_ );

    BOOST_CHECK_GT( count_data( []( const test::advertising_data& scheduled ) -> bool
        {
            return true;
        } ), 0 );
}

struct started_directed_advertising : bluetoe::link_layer::link_layer< small_temperature_service, test::radio, bluetoe::link_layer::connectable_directed_advertising >
{
    started_directed_advertising()
    {
        directed_advertising_address( bluetoe::link_layer::address::generate_static_random_address( 1 ) );
        this->run( gatt_server_ );
    }

    small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( stop_connectable_directed_advertising, started_directed_advertising )
{
    directed_advertising_address( bluetoe::link_layer::device_address() );

    run( gatt_server_ );
    clear_events();

    BOOST_CHECK_EQUAL( 0, count_data( []( const test::advertising_data& ) -> bool {
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
    const auto address = bluetoe::link_layer::address::generate_static_random_address( 1 );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 14
                && std::equal( &pdu[ 8 ], &pdu[ 14 ], address.begin() )
                && ( pdu[ 0 ] & 0x80 ) != 0;
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
    bluetoe::link_layer::link_layer< small_temperature_service, test::radio, bluetoe::link_layer::non_connectable_undirected_advertising >
{
    small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( starting_advertising, non_connectable_undirected_advertising )
{
    this->run( gatt_server_ );

    BOOST_CHECK_GT( advertisings().size(), 0 );
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

BOOST_FIXTURE_TEST_CASE( no_response_to_scan_request, non_connectable_undirected_advertising )
{
    respond_to(
        37, // channel
        {
            0xC3, 0x0C, // header
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // scanner address
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0  // advertiser address
        }
    );

    run( gatt_server_ );

    check_scheduling(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 1
                && ( pdu[ 0 ] & 0xf ) == 2;
        },
        "no_response_to_scan_request"
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
    bluetoe::link_layer::link_layer< small_temperature_service, test::radio, bluetoe::link_layer::scannable_undirected_advertising >
{
    small_temperature_service gatt_server_;
};

BOOST_FIXTURE_TEST_CASE( starting_advertising, scannable_undirected_advertising )
{
    this->run( gatt_server_ );

    BOOST_CHECK_GT( advertisings().size(), 0 );
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

BOOST_FIXTURE_TEST_CASE( response_to_scan_request, scannable_undirected_advertising )
{
    respond_to(
        37, // channel
        {
            0xC3, 0x0C, // header
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // scanner address
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0  // advertiser address
        }
    );

    run( gatt_server_ );

    BOOST_CHECK_EQUAL( 1, count_data(
        [&]( const test::advertising_data& data )
        {
            const auto& pdu = data.transmitted_data;

            return pdu.size() >= 1
                && ( pdu[ 0 ] & 0xf ) == 4;
        } ) );
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
        small_temperature_service, test::radio,
        bluetoe::link_layer::connectable_undirected_advertising,
        bluetoe::link_layer::scannable_undirected_advertising
    >
{
    small_temperature_service gatt_server_;
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

    BOOST_CHECK_GT( number_of_advertising_pdus, 1 );
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
        }), 0 );
}

BOOST_FIXTURE_TEST_CASE( empty_reponds_to_a_scan_request, scannable_and_connectable_advertising )
{
    respond_to(
        37, // channel
        {
            0xC3, 0x0C, // header
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // scanner address
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0  // advertiser address
        }
    );

    run( gatt_server_ );

    static const std::vector< std::uint8_t > expected_response =
    {
        0x44, 0x06,
        0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0
    };

    find_scheduling(
        []( const test::advertising_data& pdu ) -> bool
        {
            return pdu.channel == 37
                && pdu.transmision_time.zero()
                && pdu.transmitted_data == expected_response
                && pdu.receive_buffer.empty();
        },
        "empty_reponds_to_a_scan_request"
    );
}

BOOST_AUTO_TEST_SUITE_END()
