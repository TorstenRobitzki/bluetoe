#ifndef BLUETOE_TESTS_CHARACTERISTICS_HPP
#define BLUETOE_TESTS_CHARACTERISTICS_HPP

#include <bluetoe/characteristic_value.hpp>

namespace {
    std::uint32_t       simple_value       = 0xaabbccdd;
    const std::uint32_t simple_const_value = 0xaabbccdd;

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
        bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >
    > simple_char;

   typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid16< 0xD0B1 >,
        bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >
    > short_uuid_char;

    typedef bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
        bluetoe::bind_characteristic_value< const std::uint32_t, &simple_const_value >
    > simple_const_char;

    template < typename Char >
    struct read_characteristic_properties : Char
    {
        read_characteristic_properties()
        {
            const bluetoe::details::attribute value_attribute = this->template attribute_at< bluetoe::details::type_list<>, 0 >( 0 );
            std::uint8_t buffer[ 100 ];
            auto read = bluetoe::details::attribute_access_arguments::read( buffer, 0 );

            BOOST_REQUIRE( bluetoe::details::attribute_access_result::success == value_attribute.access( read, 1 ) );
            properties = buffer[ 0 ];
        }

        std::uint8_t properties;
    };

}

#endif // include guard
