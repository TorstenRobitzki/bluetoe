#include <iostream>
#include <iterator>

#include <bluetoe/characteristic_value.hpp>
#include <bluetoe/characteristic.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/server.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <boost/mpl/list.hpp>
#include <boost/mpl/insert_range.hpp>

#include "hexdump.hpp"
#include "test_attribute_access.hpp"
#include "test_tuple.hpp"

static const std::uint8_t test_read_value[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

using cccd_indices = std::tuple<>;
using suuid = bluetoe::service_uuid16< 0x4711 >;
struct srv {};

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

struct write_blob_handler_class {
    std::uint8_t write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value )
    {
        return write_blob_test_value_handler( offset, write_size, value );
    }

    std::uint8_t write_handler_c( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) const
    {
        return write_blob_test_value_handler( offset, write_size, value );
    }

    std::uint8_t write_handler_v( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) volatile
    {
        return write_blob_test_value_handler( offset, write_size, value );
    }

    std::uint8_t write_handler_cv( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) const volatile
    {
        return write_blob_test_value_handler( offset, write_size, value );
    }

} write_blob_handler_instance;

struct write_handler_class {
    std::uint8_t write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        return write_test_value_handler( write_size, value );
    }

    std::uint8_t write_handler_c( std::size_t write_size, const std::uint8_t* value ) const
    {
        return write_test_value_handler( write_size, value );
    }

    std::uint8_t write_handler_v( std::size_t write_size, const std::uint8_t* value ) volatile
    {
        return write_test_value_handler( write_size, value );
    }

    std::uint8_t write_handler_cv( std::size_t write_size, const std::uint8_t* value ) const volatile
    {
        return write_test_value_handler( write_size, value );
    }

} write_handler_instance;

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
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::read_blob_handler_v< read_blob_handler_class, read_blob_handler_instance, &read_blob_handler_class::read_handler_v >,
            bluetoe::write_blob_handler< write_blob_handler_class, write_blob_handler_instance, &write_blob_handler_class::write_handler >
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
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::write_blob_handler< write_blob_handler_class, write_blob_handler_instance, &write_blob_handler_class::write_handler >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::write_blob_handler_c< write_blob_handler_class, write_blob_handler_instance, &write_blob_handler_class::write_handler_c >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::write_blob_handler_v< write_blob_handler_class, write_blob_handler_instance, &write_blob_handler_class::write_handler_v >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::write_blob_handler_cv< write_blob_handler_class, write_blob_handler_instance, &write_blob_handler_class::write_handler_cv >
        >
    > write_blob_handler_tests;

typedef boost::mpl::list<
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::free_raw_write_handler< &write_test_value_handler >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::write_handler< write_handler_class, write_handler_instance, &write_handler_class::write_handler >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::write_handler_c< write_handler_class, write_handler_instance, &write_handler_class::write_handler_c >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::write_handler_v< write_handler_class, write_handler_instance, &write_handler_class::write_handler_v >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1212 >,
            bluetoe::write_handler_cv< write_handler_class, write_handler_instance, &write_handler_class::write_handler_cv >
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
    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );

    BOOST_CHECK_EQUAL( access.buffer_size, sizeof( test_read_value ) );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( test_read_value ), std::end( test_read_value ), &buffer[ 0 ], &buffer[ access.buffer_size ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_no_write_access_read_blob_handlers, Attribute, all_read_only_handler_tests )
{
    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 1 );

    auto access = bluetoe::details::attribute_access_arguments::write( test_read_value );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::write_not_permitted );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_read_blob_with_offset, Attribute, read_blob_handler_tests )
{
    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 2 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );

    BOOST_CHECK_EQUAL( access.buffer_size, sizeof( test_read_value ) - 2 );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( test_read_value ) + 2, std::end( test_read_value ), &buffer[ 0 ], &buffer[ access.buffer_size ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_read_with_offset, Attribute, read_handler_tests )
{
    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 2 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::attribute_not_long );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_read_handler_characteristic_declaration_uuid, Attribute, read_handler_tests )
{
    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 0 );

    BOOST_CHECK_EQUAL( attr.uuid, 0x2803 );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_read_handler_characteristic_declaration_value, Attribute, read_handler_tests )
{
    access_attributes< Attribute >().compare_characteristic( { 0x02, 0x02, 0x00, 0x12, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_CASE( test_read_handler_characteristic_declaration_value_for_notifyable )
{
    using read_handler_with_notification = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::notify,
        bluetoe::free_read_handler< &read_test_value_handler >
    >;

    access_attributes< read_handler_with_notification >().compare_characteristic( { 0x12, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_CASE( test_read_handler_characteristic_declaration_value_for_indication )
{
    using read_handler_with_notification = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::indicate,
        bluetoe::free_read_handler< &read_test_value_handler >
    >;

    access_attributes< read_handler_with_notification >().compare_characteristic( { 0x22, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_CASE( test_read_handler_characteristic_declaration_value_for_indication_and_notification )
{
    using read_handler_with_notification = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::notify,
        bluetoe::indicate,
        bluetoe::free_read_handler< &read_test_value_handler >
    >;

    access_attributes< read_handler_with_notification >().compare_characteristic( { 0x32, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_CASE( test_not_readable_read_handler_characteristic_declaration_value_for_notification )
{
    using read_handler_with_notification = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::notify,
        bluetoe::no_read_access,
        bluetoe::free_read_handler< &read_test_value_handler >
    >;

    access_attributes< read_handler_with_notification >().compare_characteristic( { 0x10, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
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
    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 1 );

    auto access = bluetoe::details::attribute_access_arguments::write( fixture );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( fixture ), std::end( fixture ), &test_write_value[ 0 ], &test_write_value[ sizeof( fixture ) ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_no_read_access_to_write_handler, Attribute, all_write_only_handler_tests )
{
    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 1 );
    std::uint8_t    buffer[ 100 ];

    auto access = bluetoe::details::attribute_access_arguments::read( buffer, 0 );
    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::read_not_permitted );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_write_blob_with_offset, Attribute, all_write_blob_handler_tests )
{
    static const std::uint8_t fixture[] = { 0xaa, 0xbb, 0xcc };
    static const std::uint8_t expected_value[] = { 0x00, 0x00, 0xaa, 0xbb, 0xcc };

    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 1 );

    auto access = bluetoe::details::attribute_access_arguments::write( fixture, 2 );

    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::success );
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( expected_value ), std::end( expected_value ), &test_write_value[ 0 ], &test_write_value[ sizeof( expected_value ) ] );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_write_with_offset, Attribute, write_handler_tests )
{
    static const std::uint8_t fixture[] = { 0xaa, 0xbb, 0xcc };

    const auto attr = Attribute::template attribute_at< cccd_indices, 0, suuid, srv >( 1 );
    auto access = bluetoe::details::attribute_access_arguments::write( fixture, 2 );

    BOOST_CHECK( attr.access( access, 1 ) == bluetoe::details::attribute_access_result::attribute_not_long );
}

BOOST_AUTO_TEST_CASE( test_write_handler_characteristic_declaration_value )
{
    using write_handler = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::free_raw_write_handler< &write_test_value_handler >
    >;

    access_attributes< write_handler >().compare_characteristic( { 0x08, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_CASE( test_write_handler_characteristic_declaration_value_for_notifyable )
{
    using write_handler = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::notify,
        bluetoe::free_read_handler< &read_test_value_handler >,
        bluetoe::free_raw_write_handler< &write_test_value_handler >
    >;

    access_attributes< write_handler >().compare_characteristic( { 0x1a, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_CASE( test_write_handler_characteristic_declaration_value_for_indication )
{
    using write_handler = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::indicate,
        bluetoe::free_raw_write_handler< &write_test_value_handler >,
        bluetoe::free_read_handler< &read_test_value_handler >
    >;

    access_attributes< write_handler >().compare_characteristic( { 0x2a, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_CASE( test_write_handler_characteristic_declaration_value_for_indication_and_notification )
{
    using write_handler = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::notify,
        bluetoe::indicate,
        bluetoe::free_raw_write_handler< &write_test_value_handler >,
        bluetoe::free_read_handler< &read_test_value_handler >
    >;

    access_attributes< write_handler >().compare_characteristic( { 0x3a, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_CASE( test_not_readable_write_handler_characteristic_declaration_value_for_notification )
{
    using write_handler = bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1215 >,
        bluetoe::notify,
        bluetoe::indicate,
        bluetoe::no_read_access,
        bluetoe::free_read_handler< &read_test_value_handler >,
        bluetoe::free_raw_write_handler< &write_test_value_handler >
    >;

    access_attributes< write_handler >().compare_characteristic( { 0x38, 0x02, 0x00, 0x15, 0x12 }, 0x2803 );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( mixin_handlers )

struct write_mixin
{
    write_mixin()
        : write_size( 0 )
        , value( nullptr )
    {
    }

    std::uint8_t write_handler( std::size_t w, const std::uint8_t* v )
    {
        write_size = w;
        value      = v;

        return 42;
    }

    std::size_t         write_size;
    const std::uint8_t* value;
};

using writeable_char = bluetoe::characteristic<
    bluetoe::characteristic_uuid16< 0x1234 >,
    bluetoe::mixin_write_handler< write_mixin, &write_mixin::write_handler >
>;

using write_mixin_server = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0xF8C90690, 0x3BFE, 0x4303, 0x8CE9, 0xC30C024987C8 >,
        writeable_char,
        bluetoe::mixin< write_mixin >
    >
>;

BOOST_FIXTURE_TEST_CASE( write_to_mixin, write_mixin_server )
{
    static const std::uint8_t fixture[] = { 0xaa, 0xbb, 0xcc };

    const auto attr = writeable_char::attribute_at< cccd_indices, 0, bluetoe::characteristic_uuid16< 0x1234 >, write_mixin_server >( 1 );
    auto access = bluetoe::details::attribute_access_arguments::write( fixture );
    access.server = static_cast< write_mixin_server* >( this );

    BOOST_CHECK_EQUAL( static_cast< std::int_fast16_t >( attr.access( access, 0 ) ), 42 );
    BOOST_CHECK_EQUAL( write_size, sizeof(fixture) );
    BOOST_CHECK( value == &fixture[ 0 ] );
}

struct read_mixin
{
    std::uint8_t read_handler( std::size_t, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        static constexpr std::uint8_t output[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

        std::copy( std::begin( output ), std::end( output ), out_buffer );
        out_size = std::distance( std::begin( output ), std::end( output ) );

        return 33;
    }
};

using readable_char = bluetoe::characteristic<
    bluetoe::characteristic_uuid16< 0x1234 >,
    bluetoe::mixin_read_handler< read_mixin, &read_mixin::read_handler >
>;

using read_mixin_server = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0xF8C90690, 0x3BFE, 0x4303, 0x8CE9, 0xC30C024987C8 >,
        readable_char,
        bluetoe::mixin< read_mixin >
    >
>;

BOOST_FIXTURE_TEST_CASE( read_from_mixin, read_mixin_server )
{
    std::uint8_t buffer[ 100 ];

    const auto attr = readable_char::attribute_at< cccd_indices, 0, bluetoe::characteristic_uuid16< 0x1234 >, read_mixin_server >( 1 );
    auto read       = bluetoe::details::attribute_access_arguments::read( std::begin( buffer ), std::end( buffer ), 0, bluetoe::details::client_characteristic_configuration(), static_cast< read_mixin_server* >( this ) );

    static constexpr std::uint8_t expected[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

    BOOST_CHECK_EQUAL( static_cast< std::int_fast16_t >( attr.access( read, 0 ) ), 33 );
    BOOST_CHECK_EQUAL( read.buffer_size, sizeof(expected) );
    BOOST_CHECK_EQUAL_COLLECTIONS( &buffer[ 0 ], &buffer[ read.buffer_size ], std::begin( expected ), std::end( expected ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( free_write_handler_bool )

    bool written_bool = false;
    bool free_write_bool_handler_called = false;
    std::uint8_t free_write_result = 0;

    std::uint8_t free_write_bool_handler( bool b )
    {
        BOOST_REQUIRE( !free_write_bool_handler_called );

        written_bool = b;
        free_write_bool_handler_called = true;
        return free_write_result;
    }

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0x1245 >,
        bluetoe::free_write_handler< bool, &free_write_bool_handler >
    > free_write_bool_char;

    template < class Attr >
    struct reset_bool : access_attributes< Attr >
    {
        reset_bool()
        {
            written_bool                    = false;
            free_write_bool_handler_called  = false;
            free_write_result               = 0;
        }
    };

    BOOST_FIXTURE_TEST_CASE( write_bool_with_offset, reset_bool< free_write_bool_char > )
    {
        BOOST_CHECK( write_attribute_at( { 0x00, 0x00, 0x00, 0x00 }, 1, 1 ) == bluetoe::details::attribute_access_result::attribute_not_long );
        BOOST_CHECK( !free_write_bool_handler_called );
    }

    BOOST_FIXTURE_TEST_CASE( write_bool_false, reset_bool< free_write_bool_char > )
    {
        BOOST_CHECK( write_attribute_at( { 0x00 } ) == bluetoe::details::attribute_access_result::success );
        BOOST_CHECK( written_bool == false );
        BOOST_CHECK( free_write_bool_handler_called );
    }

    BOOST_FIXTURE_TEST_CASE( write_bool_true, reset_bool< free_write_bool_char > )
    {
        BOOST_CHECK( write_attribute_at( { 0x01 } ) == bluetoe::details::attribute_access_result::success );
        BOOST_CHECK( written_bool == true );
        BOOST_CHECK( free_write_bool_handler_called );
    }

    BOOST_FIXTURE_TEST_CASE( write_bool_invalid, reset_bool< free_write_bool_char > )
    {
        BOOST_CHECK( static_cast< bluetoe::error_codes::error_codes >( write_attribute_at( { 0x06 } ) ) == bluetoe::error_codes::out_of_range );
        BOOST_CHECK( !free_write_bool_handler_called );
    }

    BOOST_FIXTURE_TEST_CASE( write_bool_to_small, reset_bool< free_write_bool_char > )
    {
        BOOST_CHECK( static_cast< bluetoe::error_codes::error_codes >( write_attribute_at( {} ) ) == bluetoe::error_codes::invalid_attribute_value_length );
        BOOST_CHECK( !free_write_bool_handler_called );
    }

    BOOST_FIXTURE_TEST_CASE( write_bool_to_large, reset_bool< free_write_bool_char > )
    {
        BOOST_CHECK( static_cast< bluetoe::error_codes::error_codes >( write_attribute_at( { 0x00, 0x00 } ) ) == bluetoe::error_codes::invalid_attribute_value_length );
        BOOST_CHECK( !free_write_bool_handler_called );
    }

    BOOST_FIXTURE_TEST_CASE( write_bool_passing_return_value, reset_bool< free_write_bool_char > )
    {
        free_write_result = 32;
        BOOST_CHECK( write_attribute_at( { 0x00 } ) == static_cast< bluetoe::details::attribute_access_result >( 32 ) );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( free_write_handler )


    // that's how JSON looks in C++ ;-)
    using supported_types = boost::mpl::list<
        std::tuple< std::uint8_t,
            // valid pairs
            std::tuple<
                std::tuple< test::tuple< 0x22 >, std::integral_constant< std::uint8_t, 34 > >,
                std::tuple< test::tuple< 0x00 >, std::integral_constant< std::uint8_t, 0 > >,
                std::tuple< test::tuple< 0xff >, std::integral_constant< std::uint8_t, 255 > >
            >,
            // wrong size examples
            std::tuple<
                test::tuple<>, test::tuple< 0x00, 0x00 >
            >
        >,
        std::tuple< std::int8_t,
            // valid pairs
            std::tuple<
                std::tuple< test::tuple< 0x22 >, std::integral_constant< std::int8_t, 34 > >,
                std::tuple< test::tuple< 0x00 >, std::integral_constant< std::int8_t, 0 > >,
                std::tuple< test::tuple< 0x80 >, std::integral_constant< std::int8_t, -128 > >,
                std::tuple< test::tuple< 0xff >, std::integral_constant< std::int8_t, -1 > >
            >,
            // wrong size examples
            std::tuple<
                test::tuple<>, test::tuple< 0x00, 0x00 >
            >
        >,
        std::tuple< std::uint16_t,
            // valid pairs
            std::tuple<
                std::tuple< test::tuple< 0x22, 0x44 >, std::integral_constant< std::uint16_t, 0x4422 > >,
                std::tuple< test::tuple< 0x00, 0x00 >, std::integral_constant< std::uint16_t, 0x0000 > >,
                std::tuple< test::tuple< 0xff, 0xff >, std::integral_constant< std::uint16_t, 0xffff > >
            >,
            // wrong size examples
            std::tuple<
                test::tuple< 0x00 >, test::tuple< 0x00, 0x00, 0x00 >
            >
        >,
        std::tuple< std::int16_t,
            // valid pairs
            std::tuple<
                std::tuple< test::tuple< 0x22, 0x44 >, std::integral_constant< std::int16_t, 0x4422 > >,
                std::tuple< test::tuple< 0x00, 0x00 >, std::integral_constant< std::int16_t, 0x0000 > >,
                std::tuple< test::tuple< 0xff, 0xff >, std::integral_constant< std::int16_t, -1 > >
            >,
            // wrong size examples
            std::tuple<
                test::tuple< 0x00 >, test::tuple< 0x00, 0x00, 0x00 >
            >
        >,
        std::tuple< std::uint32_t,
            // valid pairs
            std::tuple<
                std::tuple< test::tuple< 0xF0, 0x1A, 0x22, 0x44 >, std::integral_constant< std::uint32_t, 0x44221AF0 > >,
                std::tuple< test::tuple< 0x00, 0x00, 0x00, 0x00 >, std::integral_constant< std::uint32_t, 0x00000000 > >,
                std::tuple< test::tuple< 0xff, 0xff, 0xff, 0xff >, std::integral_constant< std::uint32_t, 0xFFFFFFFF > >
            >,
            // wrong size examples
            std::tuple<
                test::tuple< 0x00, 0x00, 0x00 >,
                test::tuple< 0x00, 0x00, 0x00, 0x00, 0x00 >
            >
        >,
        std::tuple< std::int32_t,
            // valid pairs
            std::tuple<
                std::tuple< test::tuple< 0xF0, 0x1A, 0x22, 0x44 >, std::integral_constant< std::int32_t, 0x44221AF0 > >,
                std::tuple< test::tuple< 0x00, 0x00, 0x00, 0x00 >, std::integral_constant< std::int32_t, 0x00000000 > >,
                std::tuple< test::tuple< 0xff, 0xff, 0xff, 0xff >, std::integral_constant< std::int32_t, -1 > >
            >,
            // wrong size examples
            std::tuple<
                test::tuple< 0x00, 0x00, 0x00 >,
                test::tuple< 0x00, 0x00, 0x00, 0x00, 0x00 >
            >
        >,
        std::tuple< std::uint64_t,
            // valid pairs
            std::tuple<
                std::tuple< test::tuple< 0xF0, 0x1A, 0x22, 0x44, 0x01, 0x02, 0x03, 0x04 >, std::integral_constant< std::uint64_t, 0x0403020144221AF0 > >,
                std::tuple< test::tuple< 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 >, std::integral_constant< std::uint64_t, 0x0000000000000000 > >,
                std::tuple< test::tuple< 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff >, std::integral_constant< std::uint64_t, 0xFFFFFFFFFFFFFFFF > >
            >,
            // wrong size examples
            std::tuple<
                test::tuple< 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 >,
                test::tuple< 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 >
            >
        >,
        std::tuple< std::int64_t,
            // valid pairs
            std::tuple<
                std::tuple< test::tuple< 0xF0, 0x1A, 0x22, 0x44, 0x01, 0x02, 0x03, 0x04 >, std::integral_constant< std::int64_t, 0x0403020144221AF0 > >,
                std::tuple< test::tuple< 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 >, std::integral_constant< std::int64_t, 0x0000000000000000 > >,
                std::tuple< test::tuple< 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff >, std::integral_constant< std::int64_t, -1 > >
            >,
            // wrong size examples
            std::tuple<
                test::tuple< 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 >,
                test::tuple< 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 >
            >
        >
    >;

    template < typename T >
    struct free_write_handler
    {
        static void init()
        {
            result          = 0;
            written_value   = T{};
            handler_called  = false;
        }

        free_write_handler()
        {
            init();
        };

        static std::uint8_t result;
        static T            written_value;
        static bool         handler_called;

        static std::uint8_t handler( T v )
        {
            BOOST_REQUIRE( !handler_called );
            written_value  = v;
            handler_called = true;

            return result;
        }
    };

    template < typename T >
    std::uint8_t free_write_handler< T >::result;

    template < typename T >
    T            free_write_handler< T >::written_value;

    template < typename T >
    bool         free_write_handler< T >::handler_called;


    template < class T >
    struct fixture :
        free_write_handler< T >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid16< 0x1245 >,
            bluetoe::free_write_handler<
                T, &free_write_handler< T >::handler
            >
        >
    {
    };

    template < typename World >
    struct test_valid_value : access_attributes< World >
    {
        template< typename Pair >
        void each()
        {
            this->init();

            using Input          = typename std::tuple_element< 0, Pair >::type;
            using ExpectedOutput = typename std::tuple_element< 1, Pair >::type;

            const std::vector< std::uint8_t > data = Input::values();

            BOOST_CHECK( this->write_attribute_at( &data[ 0 ], &data[ data.size() ] ) == bluetoe::details::attribute_access_result::success );
            BOOST_CHECK_EQUAL( ExpectedOutput::value, this->written_value );
            BOOST_CHECK( this->handler_called );
        }
    };

    BOOST_AUTO_TEST_CASE_TEMPLATE( write_free_handlers_check_valid_values, Fixtures, supported_types )
    {
        using Type          = typename std::tuple_element< 0, Fixtures >::type;
        using ValidValues   = typename std::tuple_element< 1, Fixtures >::type;

        fixture< Type > world;

        bluetoe::details::for_< ValidValues >::each( test_valid_value< fixture< Type > >() );
    }

    template < typename World >
    struct test_wrong_size : access_attributes< World >
    {
        template< typename Input >
        void each()
        {
            this->init();

            const std::vector< std::uint8_t > data = Input::values();

            BOOST_CHECK( this->write_attribute_at( &data[ 0 ], &data[ data.size() ] ) == bluetoe::details::attribute_access_result::invalid_attribute_value_length );
            BOOST_CHECK( !this->handler_called );
        }
    };

    BOOST_AUTO_TEST_CASE_TEMPLATE( write_free_handlers_wrong_size, Fixtures, supported_types )
    {
        using Type          = typename std::tuple_element< 0, Fixtures >::type;
        using InValidValues = typename std::tuple_element< 2, Fixtures >::type;

        fixture< Type > world;

        bluetoe::details::for_< InValidValues >::each( test_wrong_size< fixture< Type > >() );
    }

    template < typename World >
    struct test_return_value : access_attributes< World >
    {
        template< typename Pair >
        void each()
        {
            this->init();
            this->result = 42;

            using Input          = typename std::tuple_element< 0, Pair >::type;

            const std::vector< std::uint8_t > data = Input::values();

            BOOST_CHECK( static_cast< int >( this->write_attribute_at( &data[ 0 ], &data[ data.size() ] ) ) == 42 );
        }
    };

    BOOST_AUTO_TEST_CASE_TEMPLATE( write_free_handlers_check_return_value, Fixtures, supported_types )
    {
        using Type          = typename std::tuple_element< 0, Fixtures >::type;
        using ValidValues   = typename std::tuple_element< 1, Fixtures >::type;

        fixture< Type > world;

        bluetoe::details::for_< ValidValues >::each( test_return_value< fixture< Type > >() );
    }

BOOST_AUTO_TEST_SUITE_END()
