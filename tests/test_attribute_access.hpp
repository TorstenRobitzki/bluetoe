#ifndef BLUETOE_TESTS_TEST_ATTRIBUTE_ACCESS_HPP
#define BLUETOE_TESTS_TEST_ATTRIBUTE_ACCESS_HPP

#include <bluetoe/attribute.hpp>

template < typename Char >
class access_attributes : public Char, public bluetoe::details::client_characteristic_configurations< Char::number_of_client_configs >
{
public:
    using cccd_indices = std::tuple<>;

    std::pair< bool, bluetoe::details::attribute > find_attribute_by_type( std::uint16_t type )
    {
        for ( int index = 0; index != this->number_of_attributes; ++index )
        {
            const bluetoe::details::attribute value_attribute = this->template attribute_at< cccd_indices, 0, suuid, srv >( index );

            if ( value_attribute.uuid == type )
            {
                return std::make_pair( true, value_attribute );
            }
        }

        return std::pair< bool, bluetoe::details::attribute >( false, bluetoe::details::attribute{ 0, nullptr } );
    }

    bluetoe::details::attribute attribute_by_type( std::uint16_t type )
    {
        for ( int index = 0; index != this->number_of_attributes; ++index )
        {
            const bluetoe::details::attribute value_attribute = this->template attribute_at< cccd_indices, 0, suuid, srv >( index );

            if ( value_attribute.uuid == type )
            {
                return value_attribute;
            }
        }

        BOOST_REQUIRE( !"Type not found" );

        return bluetoe::details::attribute{ 0, 0 };
    }

    void compare_characteristic_at( const std::initializer_list< std::uint8_t >& input, std::size_t index )
    {
        BOOST_REQUIRE(
            bluetoe::details::attribute_access_result::success
                == read_characteristic_impl( input, this->template attribute_at< cccd_indices, 0, suuid, srv >( index ) ) );
    }

    void compare_characteristic( const std::initializer_list< std::uint8_t >& input, std::uint16_t type )
    {
        BOOST_REQUIRE(
            bluetoe::details::attribute_access_result::success
                == read_characteristic_impl( input, attribute_by_type( type ) ) );
    }

    // @TODO: proper name would be "read_attribute_at"
    bluetoe::details::attribute_access_result read_characteristic_at( const std::initializer_list< std::uint8_t >& input,
        std::size_t index, std::size_t offset, std::size_t buffer_size )
    {
        return read_characteristic_impl( input, this->template attribute_at< cccd_indices, 0, suuid, srv >( index ), offset, buffer_size );
    }

    bluetoe::details::attribute_access_result write_attribute_at( const std::uint8_t* begin, const std::uint8_t* end, std::size_t index = 1, std::size_t offset = 0 )
    {
        auto write = bluetoe::details::attribute_access_arguments::write(
            begin, end, offset, bluetoe::details::client_characteristic_configuration(), nullptr );

        return this->template attribute_at< cccd_indices, 0, suuid, srv >( index ).access( write, index );
    }

    bluetoe::details::attribute_access_result write_attribute_at( const std::initializer_list< std::uint8_t >& input, std::size_t index = 1, std::size_t offset = 0 )
    {
        return write_attribute_at( input.begin(), input.end(), index, offset );
    }

private:
    bluetoe::details::attribute_access_result read_characteristic_impl(
        const std::initializer_list< std::uint8_t >& input,
        const bluetoe::details::attribute& value_attribute,
        std::size_t offset = 0,
        std::size_t buffer_size = 100 )
    {
        const std::vector< std::uint8_t > values( input );

        std::uint8_t buffer[ 1000 ];
        auto read = bluetoe::details::attribute_access_arguments::read( &buffer[ 0 ], &buffer[ buffer_size ], offset, this->client_configurations(), nullptr );

        auto result = value_attribute.access( read, 1 );
        BOOST_REQUIRE_EQUAL_COLLECTIONS( values.begin(), values.end(), &read.buffer[ 0 ], &read.buffer[ read.buffer_size ] );

        return result;
    }

    using suuid = bluetoe::service_uuid16< 0x4711 >;
    struct srv;
};


#endif
