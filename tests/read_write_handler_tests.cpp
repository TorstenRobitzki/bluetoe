#include <bluetoe/characteristic_value.hpp>
#include <bluetoe/characteristic.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <boost/mpl/list.hpp>
#include <boost/mpl/insert_range.hpp>

static const std::uint8_t test_read_value[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

std::uint8_t read_blob_test_value_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
{
    if ( offset > sizeof( test_read_value ) )
        return bluetoe::error_codes::invalid_offset;

    out_size = std::min( sizeof( test_read_value ) - offset, read_size );
    std::copy( &test_read_value[ offset ], &test_read_value[ offset + out_size ], out_buffer );

    return bluetoe::error_codes::success;
}

std::uint8_t read_test_value_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
{
    return read_blob_test_value_handler( 0, read_size, out_buffer, out_size );
}

static std::uint8_t test_write_value[ 6 ];

std::uint8_t write_blob_test_value_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value )
{
    if ( offset > sizeof( test_write_value ) )
        return bluetoe::error_codes::invalid_offset;

    const std::size_t copy_size = std::min( sizeof( test_write_value ) - offset, write_size );
    std::copy( value, value + copy_size, std::begin( test_write_value ) + offset );

    return bluetoe::error_codes::success;
}

std::uint8_t write_test_value_handler( std::size_t write_size, const std::uint8_t* value )
{
    return write_blob_test_value_handler( 0, write_size, value );
}

struct read_blob_handler_class {

    std::uint8_t read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        return read_blob_test_value_handler( offset, read_size, out_buffer, out_size );
    }

    std::uint8_t read_handler_c( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const
    {
        return read_blob_test_value_handler( offset, read_size, out_buffer, out_size );
    }

    std::uint8_t read_handler_v( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) volatile
    {
        return read_blob_test_value_handler( offset, read_size, out_buffer, out_size );
    }

    std::uint8_t read_handler_cv( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const volatile
    {
        return read_blob_test_value_handler( offset, read_size, out_buffer, out_size );
    }

} read_blob_handler_instance;

struct read_handler_class {

    std::uint8_t read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        return read_test_value_handler( read_size, out_buffer, out_size );
    }

    std::uint8_t read_handler_c( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const
    {
        return read_test_value_handler( read_size, out_buffer, out_size );
    }

    std::uint8_t read_handler_v( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) volatile
    {
        return read_test_value_handler( read_size, out_buffer, out_size );
    }

    std::uint8_t read_handler_cv( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const volatile
    {
        return read_test_value_handler( read_size, out_buffer, out_size );
    }

} read_handler_instance;

typedef boost::mpl::list<
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::free_read_blob_handler< &read_blob_test_value_handler >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_blob_handler< read_blob_handler_class, read_blob_handler_instance, &read_blob_handler_class::read_handler >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_blob_handler_c< read_blob_handler_class, read_blob_handler_instance, &read_blob_handler_class::read_handler_c >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_blob_handler_v< read_blob_handler_class, read_blob_handler_instance, &read_blob_handler_class::read_handler_v >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_blob_handler_cv< read_blob_handler_class, read_blob_handler_instance, &read_blob_handler_class::read_handler_cv >
        >
    > read_blob_handler_tests;

typedef boost::mpl::list<
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::free_read_blob_handler< &read_blob_test_value_handler >,
            bluetoe::free_write_blob_handler< &write_blob_test_value_handler >
        >
    > read_write_blob_handler_tests;

typedef boost::mpl::list<
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::free_read_handler< &read_test_value_handler >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_handler< read_handler_class, read_handler_instance, &read_handler_class::read_handler >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_handler_c< read_handler_class, read_handler_instance, &read_handler_class::read_handler_c >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_handler_v< read_handler_class, read_handler_instance, &read_handler_class::read_handler_v >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_handler_cv< read_handler_class, read_handler_instance, &read_handler_class::read_handler_cv >
        >
    > read_handler_tests;

typedef boost::mpl::list<
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::free_write_blob_handler< &write_blob_test_value_handler >
        >
    > write_blob_handler_tests;

typedef boost::mpl::list<
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::free_write_handler< &write_test_value_handler >
        >
    > write_handler_tests;

typedef typename boost::mpl::insert_range<
    read_blob_handler_tests,
    typename boost::mpl::begin< read_blob_handler_tests >::type,
    read_write_blob_handler_tests >::type all_read_blob_handler_tests;

typedef typename boost::mpl::insert_range<
    all_read_blob_handler_tests,
    typename boost::mpl::begin< all_read_blob_handler_tests >::type,
    read_handler_tests >::type all_read_handler_tests;

typedef typename boost::mpl::insert_range<
    read_blob_handler_tests,
    typename boost::mpl::begin< read_blob_handler_tests >::type,
    read_handler_tests >::type all_read_only_handler_tests;

typedef typename boost::mpl::insert_range<
    write_blob_handler_tests,
    typename boost::mpl::begin< write_blob_handler_tests >::type,
    read_write_blob_handler_tests >::type all_write_blob_handler_tests;

typedef typename boost::mpl::insert_range<
    all_write_blob_handler_tests,
    typename boost::mpl::begin< all_write_blob_handler_tests >::type,
    write_handler_tests >::type all_write_handler_tests;

typedef typename boost::mpl::insert_range<
    write_blob_handler_tests,
    typename boost::mpl::begin< write_blob_handler_tests >::type,
    write_handler_tests >::type all_write_only_handler_tests;

BOOST_AUTO_TEST_CASE_TEMPLATE( test_read_blob_handlers, Attribute, all_read_handler_tests )
{
    const auto attr = Attribute::template attribute_at< 0 >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );

    BOOST_CHECK_EQUAL( access.buffer_size, sizeof( test_read_value ) );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( test_read_value ), std::end( test_read_value ), &buffer[ 0 ], &buffer[ access.buffer_size ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_no_write_access_read_blob_handlers, Attribute, all_read_only_handler_tests )
{
    const auto attr = Attribute::template attribute_at< 0 >( 1 );

    auto access = bluetoe::details::attribute_access_arguments::write( test_read_value );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::write_not_permitted );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_read_blob_with_offset, Attribute, read_blob_handler_tests )
{
    const auto attr = Attribute::template attribute_at< 0 >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 2 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );

    BOOST_CHECK_EQUAL( access.buffer_size, sizeof( test_read_value ) - 2 );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( test_read_value ) + 2, std::end( test_read_value ), &buffer[ 0 ], &buffer[ access.buffer_size ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_read_with_offset, Attribute, read_handler_tests )
{
    const auto attr = Attribute::template attribute_at< 0 >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 2 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::attribute_not_long );
}

struct reset_test_write_value {
    reset_test_write_value()
    {
        std::fill( std::begin( test_write_value ), std::end( test_write_value ), 0 );
    }
};

BOOST_FIXTURE_TEST_SUITE( test_write_handlers, reset_test_write_value )

BOOST_AUTO_TEST_CASE_TEMPLATE( test_write_handlers, Attribute, all_write_handler_tests )
{
    static const std::uint8_t fixture[] = { 0xaa, 0xbb, 0xcc };
    const auto attr = Attribute::template attribute_at< 0 >( 1 );

    auto access = bluetoe::details::attribute_access_arguments::write( fixture );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( fixture ), std::end( fixture ), &test_write_value[ 0 ], &test_write_value[ sizeof( fixture ) ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_no_read_access_to_write_handler, Attribute, all_write_only_handler_tests )
{
    const auto attr = Attribute::template attribute_at< 0 >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::read_not_permitted );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_write_blob_with_offset, Attribute, all_write_blob_handler_tests )
{
    static const std::uint8_t fixture[] = { 0xaa, 0xbb, 0xcc };
    static const std::uint8_t expected_value[] = { 0x00, 0x00, 0xaa, 0xbb, 0xcc };

    const auto attr = Attribute::template attribute_at< 0 >( 1 );

    auto access = bluetoe::details::attribute_access_arguments::write( fixture, 2 );

    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_value ), std::end( expected_value ), &test_write_value[ 0 ], &test_write_value[ sizeof( expected_value ) ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_write_with_offset, Attribute, write_handler_tests )
{
    static const std::uint8_t fixture[] = { 0xaa, 0xbb, 0xcc };

    const auto attr = Attribute::template attribute_at< 0 >( 1 );
    auto access = bluetoe::details::attribute_access_arguments::write( fixture, 2 );

    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::attribute_not_long );
}

BOOST_AUTO_TEST_SUITE_END()
