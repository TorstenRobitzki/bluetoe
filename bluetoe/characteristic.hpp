#ifndef BLUETOE_CHARACTERISTIC_HPP
#define BLUETOE_CHARACTERISTIC_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <cstddef>
#include <cassert>
#include <algorithm>

namespace bluetoe {

    namespace details {
        struct characteristic_meta_type;
    }

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
         */
        static details::attribute attribute_at( std::size_t index );

        typedef details::characteristic_meta_type meta_type;

    private:
        static details::attribute_access_result char_declaration_access( details::attribute_access_arguments& );

    };

    /**
     * @brief a very simple device to bind a characteristic to a global variable to provide access to the characteristic value
     */
    template < typename T, const T* >
    class bind_characteristic_value
    {
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

        static const details::attribute attributes[ number_of_attributes ] = {
            { bits( gatt_uuids::characteristic ), &char_declaration_access },
            { bits( gatt_uuids::characteristic ), nullptr }
        };

        return attributes[ index ];
    }

    template < typename ... Options >
    details::attribute_access_result characteristic< Options... >::char_declaration_access( details::attribute_access_arguments& args )
    {
        if ( args.type == details::attribute_access_type::read )
        {
            args.buffer_size = std::min< std::size_t >( args.buffer_size, 19u );

            return args.buffer_size == 19u
                ? details::attribute_access_result::success
                : details::attribute_access_result::read_truncated;
        }

        return details::attribute_access_result::write_not_permitted;
    }

}

#endif
