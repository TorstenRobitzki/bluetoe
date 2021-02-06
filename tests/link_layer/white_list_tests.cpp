#include <bluetoe/white_list.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <array>
#include <algorithm>

struct radio_without_white_list_support {
    static constexpr std::size_t radio_maximum_white_list_entries = 0;
};

template < std::size_t Size >
class mock_radio_with_white_list_support
{
public:
    static constexpr std::size_t radio_maximum_white_list_entries = Size;

    mock_radio_with_white_list_support()
        : free_size_( Size )
    {}

    std::size_t radio_white_list_free_size() const
    {
        return free_size_;
    }

    void radio_clear_white_list()
    {
        free_size_ = Size;
    }

    bool radio_add_to_white_list( const bluetoe::link_layer::device_address& addr )
    {
        if ( radio_is_in_white_list( addr ) )
            return true;

        if ( free_size_ == 0 )
            return false;

        list_[ fill_size() ] = addr;
        --free_size_;

        return true;
    }

    bool radio_remove_from_white_list( const bluetoe::link_layer::device_address& addr )
    {
        auto end = list_.begin() + fill_size();
        auto pos = std::find( list_.begin(), end, addr );

        if ( pos == end )
            return false;

        *pos = *( end - 1 );
        ++free_size_;

        return true;
    }

    bool radio_is_in_white_list( const bluetoe::link_layer::device_address& addr ) const
    {
        auto end = list_.begin() + fill_size();
        return std::find( list_.begin(), end, addr ) != end;
    }

    void radio_connection_request_filter( bool b )
    {
        connection_request_filter_ = b;
    }

    bool radio_connection_request_filter() const
    {
        return connection_request_filter_;
    }

    void radio_scan_request_filter( bool b )
    {
        scan_request_filter_ = b;
    }

    bool radio_scan_request_filter() const
    {
        return scan_request_filter_;
    }

    bool radio_is_connection_request_in_filter( const bluetoe::link_layer::device_address& addr ) const
    {
        return !connection_request_filter_ || radio_is_in_white_list( addr );
    }

    bool radio_is_scan_request_in_filter( const bluetoe::link_layer::device_address& addr ) const
    {
        return !scan_request_filter_ || radio_is_in_white_list( addr );
    }

private:
    std::size_t fill_size() const
    {
        return Size - free_size_;
    }

    std::array< bluetoe::link_layer::device_address, Size > list_;
    std::size_t                                             free_size_;
    bool                                                    connection_request_filter_;
    bool                                                    scan_request_filter_;
};

/*
 * The white list can either be implemented by hardware or by software
 */

struct only_software
    : radio_without_white_list_support
    , bluetoe::link_layer::white_list< 8 >::impl< radio_without_white_list_support, only_software >
{
};

struct only_hardware
    : mock_radio_with_white_list_support< 8 >
    , bluetoe::link_layer::white_list< 8 >::impl< mock_radio_with_white_list_support< 8 >, only_hardware >
{
};

bluetoe::link_layer::public_device_address addr1( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::random_device_address addr2( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::public_device_address addr3( { 0x02, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::random_device_address addr4( { 0x02, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::public_device_address addr5( { 0x03, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::random_device_address addr6( { 0x03, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::public_device_address addr7( { 0x04, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::random_device_address addr8( { 0x04, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::public_device_address addr9( { 0x05, 0x02, 0x03, 0x04, 0x05, 0x06 } );
bluetoe::link_layer::random_device_address addr10( { 0x05, 0x02, 0x03, 0x04, 0x05, 0x06 } );

typedef boost::mpl::list<
    only_software,
    only_hardware
> test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE( maximum_white_list_entries_is_provied, T, test_types )
{
    BOOST_CHECK_EQUAL( std::size_t(T::maximum_white_list_entries), 8u );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( reports_the_maximum_number_of_elements_after_default_contructed, T, test_types )
{
    T white_list;
    BOOST_CHECK_EQUAL( white_list.white_list_free_size(), 8u );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( adding_one_address, T, test_types )
{
    T white_list;
    BOOST_CHECK( white_list.add_to_white_list( addr1 ) );
    BOOST_CHECK_EQUAL( white_list.white_list_free_size(), 7u );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( adding_the_same_address, T, test_types )
{
    T white_list;
    BOOST_CHECK( white_list.add_to_white_list( addr1 ) );
    BOOST_CHECK( white_list.add_to_white_list( addr2 ) );
    BOOST_CHECK( white_list.add_to_white_list( addr1 ) );

    BOOST_CHECK_EQUAL( white_list.white_list_free_size(), 6u );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( addr_not_in_list, T, test_types )
{
    T white_list;
    BOOST_CHECK( !white_list.is_in_white_list( addr1 ) );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( addr_is_in_list, T, test_types )
{
    T white_list;
    white_list.add_to_white_list( addr1 );
    BOOST_CHECK( white_list.is_in_white_list( addr1 ) );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( addr_is_in_list2, T, test_types )
{
    T white_list;
    white_list.add_to_white_list( addr1 );
    white_list.add_to_white_list( addr2 );
    white_list.add_to_white_list( addr3 );
    white_list.add_to_white_list( addr4 );
    white_list.add_to_white_list( addr5 );
    BOOST_CHECK( white_list.is_in_white_list( addr3 ) );
    BOOST_CHECK( white_list.is_in_white_list( addr5 ) );
    BOOST_CHECK( !white_list.is_in_white_list( addr6 ) );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( remove_from_empty, T, test_types )
{
    T white_list;
    BOOST_CHECK( !white_list.remove_from_white_list( addr1 ) );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( remove_from_list, T, test_types )
{
    T white_list;
    white_list.add_to_white_list( addr1 );
    white_list.add_to_white_list( addr2 );
    white_list.add_to_white_list( addr3 );
    white_list.add_to_white_list( addr4 );
    white_list.add_to_white_list( addr5 );

    BOOST_CHECK( white_list.remove_from_white_list( addr1 ) );
    BOOST_CHECK( white_list.remove_from_white_list( addr5 ) );
    BOOST_CHECK( !white_list.remove_from_white_list( addr1 ) );
    BOOST_CHECK( !white_list.remove_from_white_list( addr5 ) );

    BOOST_CHECK( white_list.is_in_white_list( addr2 ) );
    BOOST_CHECK( white_list.is_in_white_list( addr3 ) );
    BOOST_CHECK( white_list.is_in_white_list( addr4 ) );

    BOOST_CHECK_EQUAL( white_list.white_list_free_size(), 5u );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( activate_white_list_function_exists, T, test_types )
{
    T white_list;
    white_list.connection_request_filter( true );
    white_list.scan_request_filter( true );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( clear_empty_white_list, T, test_types )
{
    T white_list;
    white_list.clear_white_list();
    BOOST_CHECK_EQUAL( white_list.white_list_free_size(), 8u );
}

BOOST_AUTO_TEST_CASE_TEMPLATE( clear_none_empty_white_list, T, test_types )
{
    T white_list;
    white_list.add_to_white_list( addr1 );
    white_list.add_to_white_list( addr2 );

    white_list.clear_white_list();
    BOOST_CHECK_EQUAL( white_list.white_list_free_size(), 8u );
    BOOST_CHECK( !white_list.is_in_white_list( addr1 ) );
    BOOST_CHECK( !white_list.is_in_white_list( addr2 ) );
}

BOOST_FIXTURE_TEST_CASE( all_connection_requests_are_in_filter_by_default, only_software )
{
    BOOST_CHECK( !connection_request_filter() );
    BOOST_CHECK( is_connection_request_in_filter( addr1 ) );
}

BOOST_FIXTURE_TEST_CASE( activation_connection_request_filter, only_software )
{
    connection_request_filter( true );
    BOOST_CHECK( !is_connection_request_in_filter( addr1 ) );
}

BOOST_FIXTURE_TEST_CASE( deactivation_connection_request_filter, only_software )
{
    connection_request_filter( true );
    connection_request_filter( false );
    BOOST_CHECK( is_connection_request_in_filter( addr1 ) );
}

BOOST_FIXTURE_TEST_CASE( address_in_activated_connection_filter, only_software )
{
    connection_request_filter( true );
    add_to_white_list( addr1 );
    BOOST_CHECK( is_connection_request_in_filter( addr1 ) );
}

BOOST_FIXTURE_TEST_CASE( all_scan_requests_are_in_filter_by_default, only_software )
{
    BOOST_CHECK( !scan_request_filter() );
    BOOST_CHECK( is_scan_request_in_filter( addr1 ) );
}

BOOST_FIXTURE_TEST_CASE( activation_scan_request_filter, only_software )
{
    scan_request_filter( true );
    BOOST_CHECK( !is_scan_request_in_filter( addr1 ) );
}

BOOST_FIXTURE_TEST_CASE( deactivation_scan_request_filter, only_software )
{
    scan_request_filter( true );
    scan_request_filter( false );
    BOOST_CHECK( is_scan_request_in_filter( addr1 ) );
}

BOOST_FIXTURE_TEST_CASE( address_in_activated_scan_filter, only_software )
{
    scan_request_filter( true );
    add_to_white_list( addr1 );
    BOOST_CHECK( is_scan_request_in_filter( addr1 ) );
}
