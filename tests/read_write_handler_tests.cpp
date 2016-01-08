#include <iostream>
#include <bluetoe/Characteristic.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <boost/mpl/list.hpp>

static const std::uint8_t test_read_value[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

std::uint8_t read_blob_test_value_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
{
    if ( offset > sizeof( test_read_value ) )
        return bluetoe::error_codes::invalid_offset;

    out_size = std::min( sizeof( test_read_value ) - offset, read_size );
    std::copy( &test_read_value[ offset ], &test_read_value[ offset + out_size ], out_buffer );

    return bluetoe::error_codes::success;
}

typedef boost::mpl::list<
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::free_read_blob_handler< &read_blob_test_value_handler >
        >
    > read_blob_handler_tests;

BOOST_AUTO_TEST_CASE_TEMPLATE( test_read_blob_handlers, Characteristic, read_blob_handler_tests )
{
    const auto attr = Characteristic::template attribute_at< 0 >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );

    BOOST_CHECK_EQUAL( access.buffer_size, sizeof( test_read_value ) );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( test_read_value ), std::end( test_read_value ), &test_read_value[ 0 ], &test_read_value[ access.buffer_size ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_no_write_access_read_blob_handlers, Characteristic, read_blob_handler_tests )
{
    const auto attr = Characteristic::template attribute_at< 0 >( 1 );

    auto access = bluetoe::details::attribute_access_arguments::write( test_read_value );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::write_not_permitted );
}