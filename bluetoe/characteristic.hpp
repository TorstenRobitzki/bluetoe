#ifndef BLUETOE_CHARACTERISTIC_HPP
#define BLUETOE_CHARACTERISTIC_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/uuid.hpp>
#include <bluetoe/options.hpp>

#include <cstddef>
#include <cassert>
#include <algorithm>

namespace bluetoe {

    namespace details {
        struct characteristic_meta_type;
        struct characteristic_uuid_meta_type;
        struct characteristic_value_metat_type;
    }

    /**
     * @brief a 128-Bit UUID used to identify a characteristic.
     *
     * The class takes 5 parameters to store the UUID in the usual form like this:
     * @code{.cpp}
     * bluetoe::characteristic_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
     * @endcode
     */
    template <
        std::uint64_t A,
        std::uint64_t B,
        std::uint64_t C,
        std::uint64_t D,
        std::uint64_t E >
    struct characteristic_uuid : details::uuid< A, B, C, D, E >
    {
        typedef details::characteristic_uuid_meta_type meta_type;
    };

    /**
     * @brief a 16-Bit UUID used to identify a characteristic.
     */
    template <
        std::uint64_t UUID >
    struct characteristic_uuid16 : details::uuid16< UUID >
    {
        typedef details::characteristic_uuid_meta_type meta_type;
    };

    /**
     * @brief a characteristic is a data point that is accessable by client.
     */
    template < typename ... Options >
    class characteristic
    {
    public:
        /**
         * a service is a list of attributes
         */
        static constexpr std::size_t number_of_attributes = 2;

        /**
         * @brief gives access to the all attributes of the characteristic
         *
         * @TODO: the "Characteristic Declaration" contains an absolute handle value of the "Characteristic Value"
         *        that "Characteristic Value" is the first attribute behind the Declaration. Two possible solutions:
         *        - extend attribute_at to take two index (a global one) and a local one.
         *        - return a wrong handle and fix the handle on a higher level
         *
         * @TODO: the "Characteristic Value Declaration" has an attribute type that can be 16 or 128 bit.
         */
        static details::attribute attribute_at( std::size_t index );

        typedef details::characteristic_meta_type meta_type;

    private:
        static details::attribute_access_result char_declaration_access( details::attribute_access_arguments& );

    };

    /**
     * @brief a very simple device to bind a characteristic to a global variable to provide access to the characteristic value
     */
    template < typename T, T* Ptr >
    class bind_characteristic_value
    {
    public:
        static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments& );
        typedef details::characteristic_value_metat_type meta_type;
    };

    /**
     * @brief adds a name to characteristic
     *
     * Adds a "Characteristic User Description" to the characteristic
     */
    template < const char* const >
    class characteristic_name
    {
    };

    /**
     * @brief if added as option to a characteristic, defines the characteristic to be read-only
     */
    class read_only {

    };

    // implementation
    template < typename ... Options >
    details::attribute characteristic< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );
        typedef typename details::find_by_meta_type< details::characteristic_value_metat_type, Options... >::type value;

        static const details::attribute attributes[ number_of_attributes ] = {
            { bits( gatt_uuids::characteristic ), &char_declaration_access },
            { bits( gatt_uuids::characteristic ), &value::characteristic_value_access }
        };

        return attributes[ index ];
    }

    template < typename ... Options >
    details::attribute_access_result characteristic< Options... >::char_declaration_access( details::attribute_access_arguments& args )
    {
        typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, Options... >::type uuid;
        static const auto uuid_offset = 3;

        if ( args.type == details::attribute_access_type::read )
        {
            static constexpr auto uuid_size = sizeof( uuid::bytes );

            args.buffer_size          = std::min< std::size_t >( args.buffer_size, uuid_offset + uuid_size );
            const auto max_uuid_bytes = std::min< std::size_t >( std::max< int >( 0, args.buffer_size -uuid_offset ), uuid_size );

            /// @TODO fill "Characteristic Properties" and "Characteristic Value Attribute Handle"
            if ( max_uuid_bytes )
                std::copy( std::begin( uuid::bytes ), std::begin( uuid::bytes ) + max_uuid_bytes, args.buffer + uuid_offset );

            return args.buffer_size == uuid_offset + uuid_size
                ? details::attribute_access_result::success
                : details::attribute_access_result::read_truncated;
        }

        return details::attribute_access_result::write_not_permitted;
    }

    template < typename T, T* Ptr >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::characteristic_value_access( details::attribute_access_arguments& args )
    {
        if ( args.type == details::attribute_access_type::read )
        {
            args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) );
            static const std::uint8_t* ptr = static_cast< const std::uint8_t* >( static_cast< const void* >( Ptr ) );
            std::copy( ptr, ptr + args.buffer_size, args.buffer );

            return args.buffer_size == sizeof( T )
                ? details::attribute_access_result::success
                : details::attribute_access_result::read_truncated;
        }
        else
        {
            args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) );
            static std::uint8_t* ptr = static_cast< std::uint8_t* >( static_cast< void* >( Ptr ) );
            std::copy( args.buffer, args.buffer + args.buffer_size, ptr );

            return args.buffer_size == sizeof( T )
                ? details::attribute_access_result::success
                : details::attribute_access_result::write_truncated;
        }

        return details::attribute_access_result::write_not_permitted;
    }

}

#endif
