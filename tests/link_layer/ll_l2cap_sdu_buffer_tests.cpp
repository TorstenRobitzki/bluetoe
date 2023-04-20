#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/ll_l2cap_sdu_buffer.hpp>

#include "test_layout.hpp"

#include <initializer_list>

using namespace boost::test_tools;

namespace {
    class radio_mock_t
    {
    public:
        radio_mock_t()
            : available_transmit_buffers_( 0 )
        {
        }

        static constexpr std::size_t header_size = 2;
        static constexpr std::size_t layout_overhead = 1;

        using layout = test::layout_with_overhead< layout_overhead >;

        bluetoe::link_layer::read_buffer allocate_transmit_buffer( std::size_t size )
        {
            // allocate_transmit_buffer() is idempotent and thus should always return the
            // same buffer.
            if ( available_transmit_buffers_ == 0 )
                return { nullptr, 0 };

            assert( size <=
                static_cast< std::size_t >( std::distance( std::begin( transmit_buffer_ ), std::end( transmit_buffer_ ) ) ) );

            return { transmit_buffer_, size };
        }

        void commit_transmit_buffer( bluetoe::link_layer::read_buffer buffer )
        {
            tranmitted_pdus_.push_back( pdu_t( buffer.buffer, buffer.buffer + buffer.size ) );
            assert( available_transmit_buffers_ > 0 );
            --available_transmit_buffers_;
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

        void add_free_ll_pdus(std::size_t num_buffers)
        {
            available_transmit_buffers_ += num_buffers;
        }

        std::size_t max_tx_size() const
        {
            return 29 + layout_overhead;
        }

        std::vector< std::uint8_t > next_transmitted_pdu()
        {
            if ( tranmitted_pdus_.empty() )
                return {};

            const auto result = tranmitted_pdus_.front();
            tranmitted_pdus_.erase( tranmitted_pdus_.begin() );

            return result;
        }

        void fill_buffer( bluetoe::link_layer::read_buffer buffer, const std::initializer_list< std::uint8_t >& data )
        {
            assert( buffer.size >= data.size() );
            std::copy( data.begin(), data.end(), buffer.buffer );
        }

    private:
        using pdu_t = std::vector< std::uint8_t >;
        std::vector< pdu_t > received_pdus_;
        std::size_t          available_transmit_buffers_;
        std::vector< pdu_t > tranmitted_pdus_;
        std::uint8_t         transmit_buffer_[256];
    };

    class buffer_under_test : public bluetoe::link_layer::ll_l2cap_sdu_buffer< radio_mock_t, 100 >
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

        void write_l2cap_size( bluetoe::link_layer::read_buffer buffer, std::size_t size )
        {
            bluetoe::details::write_16bit( layout::body( buffer ).first, size );
        }
    };
}

// All Tests are done with a layout that has an extra byte between header and body
BOOST_AUTO_TEST_SUITE( receive_buffer_for_mtu_size_larger_than_23_bytes )

    BOOST_FIXTURE_TEST_CASE( forward_empty_buffer, buffer_under_test )
    {
        expect_next_received( {} );
    }

    BOOST_FIXTURE_TEST_CASE( link_layer_controll_pdus_are_directly_handed_from_the_radio, buffer_under_test )
    {
        add_received_pdu( { 0x03, 0x03, 0xaa, 0x01, 0x02, 0x03 } );

        expect_next_received( { 0x03, 0x03, 0xaa, 0x01, 0x02, 0x03 } );
    }

    BOOST_FIXTURE_TEST_CASE( small_l2cap_sdus_are_directly_handled_by_the_radio, buffer_under_test )
    {
        add_received_pdu( { 0x02, 0x08, 0xaa, 0x04, 0x00, 0xff, 0xff, 0x03, 0x01, 0x02, 0x03 } );

        expect_next_received( { 0x02, 0x08, 0xaa, 0x04, 0x00, 0xff, 0xff, 0x03, 0x01, 0x02, 0x03 } );
    }

    BOOST_FIXTURE_TEST_CASE( free_ll_l2cap_received_consumes_received_pdu, buffer_under_test )
    {
        add_received_pdu( { 0x02, 0x04, 0xaa, 0x00, 0x00, 0x00, 0x00 } );
        expect_next_received( { 0x02, 0x04, 0xaa, 0x00, 0x00, 0x00, 0x00 } );

        free_ll_l2cap_received();
        expect_next_received( {} );
    }

    BOOST_FIXTURE_TEST_CASE( too_small_l2cap_pdus_are_ignored, buffer_under_test )
    {
        add_received_pdu( { 0x02, 0x03, 0xaa, 0x00, 0x00 } );
        expect_next_received( {} );
    }

    BOOST_FIXTURE_TEST_CASE( fragmented_pdu_exceeds_maximum_mtu, buffer_under_test )
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

    BOOST_FIXTURE_TEST_CASE( receiving_incomplete_fragmented_pdu, buffer_under_test )
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

    BOOST_FIXTURE_TEST_CASE( receiving_fragmented_pdu, buffer_under_test )
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

    BOOST_FIXTURE_TEST_CASE( receiving_fragmented_pdu_in_two_calls, buffer_under_test )
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

    struct received_24byte_fragmented_sdu : public buffer_under_test
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

// All Tests are done with a layout that has an extra byte between header and body
BOOST_AUTO_TEST_SUITE( transmit_l2cap_sdus )

    BOOST_FIXTURE_TEST_CASE( allocating_buffer, buffer_under_test )
    {
        const auto buffer = allocate_l2cap_transmit_buffer( 50 );

        BOOST_CHECK_EQUAL( buffer.size, 50u + 4u + 3u );
        BOOST_CHECK( buffer.buffer != nullptr );
    }

    BOOST_FIXTURE_TEST_CASE( allocating_min_buffer, buffer_under_test )
    {
        const auto buffer = allocate_l2cap_transmit_buffer( 0 );

        BOOST_CHECK_EQUAL( buffer.size, 4u + 3u );
        BOOST_CHECK( buffer.buffer != nullptr );
    }

    BOOST_FIXTURE_TEST_CASE( allocating_max_buffer, buffer_under_test )
    {
        const auto buffer = allocate_l2cap_transmit_buffer( 100 );

        BOOST_CHECK_EQUAL( buffer.size, 100u + 4u + 3u );
        BOOST_CHECK( buffer.buffer != nullptr );

        // touch every single byte to trigger the sanatizer
        std::fill( buffer.buffer, buffer.buffer + buffer.size, 0x42 );
    }

    // if the l2cap SDU uses all available LL PDUs, allocating a LL
    // PDU must fail
    BOOST_FIXTURE_TEST_CASE( l2cap_sdu_can_not_fully_written_to_the_ll_buffer, buffer_under_test )
    {
        const auto buffer = allocate_l2cap_transmit_buffer( 100 );
        write_l2cap_size( buffer, 100 );
        commit_l2cap_transmit_buffer( buffer );

        const auto buffer2 = allocate_ll_transmit_buffer( 30 );

        BOOST_CHECK_EQUAL( buffer2.size, 0u );
        BOOST_CHECK( buffer2.buffer == nullptr );
    }

    BOOST_FIXTURE_TEST_CASE( l2cap_sdu_can_fully_written_to_the_ll_buffer, buffer_under_test )
    {
        add_free_ll_pdus(100);
        const auto buffer = allocate_l2cap_transmit_buffer( 100 );
        write_l2cap_size( buffer, 100 );
        commit_l2cap_transmit_buffer( buffer );

        const auto buffer2 = allocate_ll_transmit_buffer( 27 );

        BOOST_CHECK_EQUAL( buffer2.size, 30u );
        BOOST_CHECK( buffer2.buffer != nullptr );
    }

    BOOST_FIXTURE_TEST_CASE( allocating_l2cap_buffer_is_possible_once_all_ll_pdus_are_written, buffer_under_test )
    {
        // With a PDU size of 30 (min 29 + 1 for the layout overheader), the 107 bytes are written in 4 chunks:
        // 30, 27, 27, 23
        add_free_ll_pdus(3);
        const auto buffer = allocate_l2cap_transmit_buffer( 100 );
        write_l2cap_size( buffer, 100 );
        commit_l2cap_transmit_buffer( buffer );

        // there is still one PDU missing to send the entire L2CAP SDU
        BOOST_CHECK_EQUAL( allocate_ll_transmit_buffer( 27 ).size, 0u );
        BOOST_CHECK_EQUAL( allocate_l2cap_transmit_buffer( 23 ).size, 0u );

        // If the entire L2CAP SDU is written, the L2CAP buffer can be used again,
        // but still no LL PDU available
        add_free_ll_pdus(1);
        BOOST_CHECK_EQUAL( allocate_ll_transmit_buffer( 27 ).size, 0u );
        BOOST_CHECK_EQUAL( allocate_l2cap_transmit_buffer( 23 ).size, 30u );

        add_free_ll_pdus(1);
        BOOST_CHECK_EQUAL( allocate_ll_transmit_buffer( 27 ).size, 30u );
        BOOST_CHECK_EQUAL( allocate_l2cap_transmit_buffer( 23 ).size, 30u );
    }

    BOOST_FIXTURE_TEST_CASE( unfragmented_sdu, buffer_under_test )
    {
        add_free_ll_pdus(1);
        const auto buffer = allocate_l2cap_transmit_buffer( 23 );
        BOOST_REQUIRE( buffer.size == 30 );

        fill_buffer( buffer, {
            0x00, 0x00, 0x00,
            0x17, 0x00, 0x04, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26
        } );

        commit_l2cap_transmit_buffer( buffer );

        BOOST_TEST( next_transmitted_pdu() == std::vector< std::uint8_t >({
            0x02, 0x1B, 0x00,
            0x17, 0x00, 0x04, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26
        }), per_element() );
    }

    BOOST_FIXTURE_TEST_CASE( small_unfragmented_sdu, buffer_under_test )
    {
        add_free_ll_pdus(1);
        const auto buffer = allocate_l2cap_transmit_buffer( 23 );
        BOOST_REQUIRE( buffer.size == 30 );

        fill_buffer( buffer, {
            0x00, 0x00, 0x00,
            0x08, 0x00, 0x04, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
        } );

        commit_l2cap_transmit_buffer( buffer );

        BOOST_TEST( next_transmitted_pdu() == std::vector< std::uint8_t >({
            0x02, 0x0C, 0x00,
            0x08, 0x00, 0x04, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
        }), per_element() );
    }

    BOOST_FIXTURE_TEST_CASE( fragmented_sdu, buffer_under_test )
    {
        add_free_ll_pdus(2);
        const auto buffer = allocate_l2cap_transmit_buffer( 100 );
        BOOST_REQUIRE( buffer.size == 107);

        fill_buffer( buffer, {
            0x00, 0x00, 0x00,
            0x48, 0x00, 0x04, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
            0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
            0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
            0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
            0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
            0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
            0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87
        } );

        commit_l2cap_transmit_buffer( buffer );

        BOOST_TEST( next_transmitted_pdu() == std::vector< std::uint8_t >({
            0x02, 0x1B, 0x00,
            0x48, 0x00, 0x04, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26
        }), per_element() );

        BOOST_TEST( next_transmitted_pdu() == std::vector< std::uint8_t >({
            0x01, 0x1B, 0x00,
                                                      0x27,
            0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
            0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
            0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
            0x60, 0x61
        }), per_element() );

        // transmitting stucked, because of no available transmit buffers in the radion
        BOOST_TEST( next_transmitted_pdu() == std::vector< std::uint8_t >());

        // make an addition transmit buffer available
        add_free_ll_pdus(1);
        // Next, when looking for new PDUs, the freed buffer is used to output
        // the pending buffer
        next_ll_l2cap_received();

        BOOST_TEST( next_transmitted_pdu() == std::vector< std::uint8_t >({
            0x01, 0x16, 0x00,
                        0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
            0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
            0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87
        }), per_element() );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( transmit_ll_pdus )

    BOOST_FIXTURE_TEST_CASE( allocating_buffer, buffer_under_test )
    {
        add_free_ll_pdus(1);

        const auto b1 = allocate_ll_transmit_buffer( 27 );
        BOOST_CHECK_EQUAL( b1.size, 27u + 2u + 1u );

        // allocate_ll_transmit_buffer is idempotent
        const auto b2 = allocate_ll_transmit_buffer( 27 );
        BOOST_CHECK_EQUAL( b2.size, 27u + 2u + 1u );

        commit_ll_transmit_buffer( b2 );

        const auto b3 = allocate_ll_transmit_buffer( 27 );
        BOOST_CHECK_EQUAL( b3.size, 0u );
    }

BOOST_AUTO_TEST_SUITE_END()
