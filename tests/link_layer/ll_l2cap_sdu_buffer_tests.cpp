#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/ll_l2cap_sdu_buffer.hpp>

#include "test_layout.hpp"

#include <initializer_list>

namespace {
    class radio_mock_t
    {
    public:
        static constexpr std::size_t header_size = 2;
        static constexpr std::size_t layout_overhead = 1;

        using layout = test::layout_with_overhead< 1 >;

        bluetoe::link_layer::read_buffer allocate_transmit_buffer( std::size_t /*size*/ )
        {
            return { nullptr, 0 };
        }

        void commit_transmit_buffer( bluetoe::link_layer::read_buffer )
        {
        }

        bluetoe::link_layer::write_buffer next_received() const
        {
            if ( received_pdus_.empty() )
                return { nullptr, 0 };

            auto& pdu = received_pdus_.front();

            return { &pdu[ 0 ], pdu.size() };
        }

        void free_received()
        {
            BOOST_REQUIRE( !received_pdus_.empty() );
            received_pdus_.erase( received_pdus_.begin() );
        }

        /*
         * Interface for the tests
         */
        void add_received_pdu( std::initializer_list< std::uint8_t > incomming_pdu )
        {
            received_pdus_.push_back( pdu_t{ incomming_pdu.begin(), incomming_pdu.end() } );
        }

        bool receive_buffer_empty() const
        {
            return received_pdus_.empty();
        }

    private:
        using pdu_t = std::vector< std::uint8_t >;
        std::vector< pdu_t > received_pdus_;
    };

    class puffer_under_test : public bluetoe::link_layer::ll_l2cap_sdu_buffer< radio_mock_t, 100 >
    {
    public:
        void expect_next_received( std::initializer_list< std::uint8_t > expected )
        {
            const auto received = next_ll_l2cap_received();

            if ( received.size == 0 || received.buffer == nullptr || expected.size() == 0 )
            {
                BOOST_TEST( received.size == 0u );
                BOOST_CHECK_EQUAL( static_cast< const void* >( received.buffer ), nullptr );
                BOOST_TEST( expected.size() == 0u );
            }
            else
            {
                BOOST_CHECK_EQUAL_COLLECTIONS( received.buffer, received.buffer + received.size, expected.begin(), expected.end() );
            }
        }

    };
}

// All Tests are done with a layout that has an extra byte between header and body
BOOST_AUTO_TEST_SUITE( receive_buffer_for_mtu_size_larger_than_23_bytes )

    BOOST_FIXTURE_TEST_CASE( forward_empty_buffer, puffer_under_test )
    {
        expect_next_received( {} );
    }

    BOOST_FIXTURE_TEST_CASE( link_layer_controll_pdus_are_directly_handed_from_the_radio, puffer_under_test )
    {
        add_received_pdu( { 0x03, 0x03, 0xaa, 0x01, 0x02, 0x03 } );

        expect_next_received( { 0x03, 0x03, 0xaa, 0x01, 0x02, 0x03 } );
    }

    BOOST_FIXTURE_TEST_CASE( small_l2cap_sdus_are_directly_handled_by_the_radio, puffer_under_test )
    {
        add_received_pdu( { 0x02, 0x08, 0xaa, 0x04, 0x00, 0xff, 0xff, 0x03, 0x01, 0x02, 0x03 } );

        expect_next_received( { 0x02, 0x08, 0xaa, 0x04, 0x00, 0xff, 0xff, 0x03, 0x01, 0x02, 0x03 } );
    }

    BOOST_FIXTURE_TEST_CASE( free_ll_l2cap_received_consumes_received_pdu, puffer_under_test )
    {
        add_received_pdu( { 0x02, 0x04, 0xaa, 0x00, 0x00, 0x00, 0x00 } );
        expect_next_received( { 0x02, 0x04, 0xaa, 0x00, 0x00, 0x00, 0x00 } );

        free_ll_l2cap_received();
        expect_next_received( {} );
    }

    BOOST_FIXTURE_TEST_CASE( too_small_l2cap_pdus_are_ignored, puffer_under_test )
    {
        add_received_pdu( { 0x02, 0x03, 0xaa, 0x00, 0x00 } );
        expect_next_received( {} );
    }

    BOOST_FIXTURE_TEST_CASE( fragmented_pdu_exceeds_maximum_mtu, puffer_under_test )
    {
        add_received_pdu(
            {
                0x02, 0x1B, 0xaa, 0x65, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 23 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
            }
        );

        add_received_pdu(
            {
                0x01, 0x1B, 0xaa,                               // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 27 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                0x31, 0x32, 0x33
            }
        );

        add_received_pdu(
            {
                0x01, 0x1B, 0xaa,                               // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 27 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                0x31, 0x32, 0x33
            }
        );

        add_received_pdu(
            {
                0x01, 0x18, 0xaa,                               // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 24 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
            }
        );

        add_received_pdu( { 0x03, 0x03, 0xaa, 0x01, 0x02, 0x03 } );

        expect_next_received( { 0x03, 0x03, 0xaa, 0x01, 0x02, 0x03 } );
    }

    BOOST_FIXTURE_TEST_CASE( receiving_incomplete_fragmented_pdu, puffer_under_test )
    {
        add_received_pdu(
            {
                0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 23 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
            }
        );

        expect_next_received( {} );
    }

    BOOST_FIXTURE_TEST_CASE( receiving_fragmented_pdu, puffer_under_test )
    {
        add_received_pdu(
            {
                0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 23 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
            }
        );

        add_received_pdu(
            {
                0x01, 0x01, 0xaa, 0x28                          // remaining byte
            }
        );

        expect_next_received(
            {
                0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 24 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
            }
        );
    }

    BOOST_FIXTURE_TEST_CASE( receiving_fragmented_pdu_in_two_calls, puffer_under_test )
    {
        add_received_pdu(
            {
                0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 23 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
            }
        );

        // after the incomplete SDU was received, the buffer returns no SDU
        expect_next_received({});

        add_received_pdu(
            {
                0x01, 0x01, 0xaa, 0x28                          // remaining byte
            }
        );

        expect_next_received(
            {
                0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 24 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
            }
        );
    }

    struct received_24byte_fragmented_sdu : public puffer_under_test
    {
        received_24byte_fragmented_sdu()
        {
            add_received_pdu(
                {
                    0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 23 bytes of data
                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
                }
            );

            add_received_pdu(
                {
                    0x01, 0x01, 0xaa, 0x28                          // remaining byte
                }
            );
        }
    };

    BOOST_FIXTURE_TEST_CASE( next_ll_l2cap_received_is_idempotent, received_24byte_fragmented_sdu )
    {
        expect_next_received(
            {
                0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 24 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
            }
        );

        expect_next_received(
            {
                0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 24 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
            }
        );
    }

    BOOST_FIXTURE_TEST_CASE( free_received_fragmented, received_24byte_fragmented_sdu )
    {
        free_ll_l2cap_received();
        expect_next_received({});
    }

    BOOST_FIXTURE_TEST_CASE( receiving_after_fragmented_sdu, received_24byte_fragmented_sdu )
    {
        add_received_pdu( { 0x03, 0x03, 0xaa, 0x01, 0x02, 0x03 } );

        expect_next_received(
            {
                0x02, 0x1D, 0xaa, 0x18, 0x00, 0x04, 0x00,       // Header: L2CAP length 24
                0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // 24 bytes of data
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
            }
        );
        free_ll_l2cap_received();

        expect_next_received( { 0x03, 0x03, 0xaa, 0x01, 0x02, 0x03 } );
        free_ll_l2cap_received();

        expect_next_received( {} );

    }
BOOST_AUTO_TEST_SUITE_END()
