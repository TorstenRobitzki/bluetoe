#ifndef BLUETOE_ATTRIBUTE_HPP
#define BLUETOE_ATTRIBUTE_HPP

#include <cstdint>
#include <cstddef>

namespace bluetoe {
namespace details {

    /*
     * Attribute and accessing an attribute
     */

    enum class attribute_access_result {
        success,
        // read just as much as was possible to write into the output buffer
        read_truncated,
        write_truncated,
        write_not_permitted
    };

    enum class attribute_access_type {
        read,
        write
    };

    struct attribute_access_arguments
    {
        attribute_access_type   type;
        std::uint8_t*           buffer;
        std::size_t             buffer_size;

        template < std::size_t N >
        static attribute_access_arguments read( std::uint8_t(&buffer)[N] )
        {
            return attribute_access_arguments{
                attribute_access_type::read,
                &buffer[ 0 ],
                N
            };
        }

        template < std::size_t N >
        static attribute_access_arguments write( const std::uint8_t(&buffer)[N] )
        {
            return attribute_access_arguments{
                attribute_access_type::write,
                const_cast< std::uint8_t* >( &buffer[ 0 ] ),
                N
            };
        }

    };

    typedef attribute_access_result ( *attribute_access )( attribute_access_arguments& );

    /*
     * An attribute is an uuid combined with a mean of how to access the attributes
     *
     * design decisions to _not_ use pointer to staticaly allocated virtual objects:
     * - makeing access a pointer to a virtual base class would result in storing a pointer that points to a pointer (vtable) to list with a bunch of functions.
     * - it's most likely that most attributes will not have any mutable data.
     * attribute contains only one function that takes a attribute_access_type to save memory and in the expectation that there are only a few different combination of
     * access functions.
     */
    struct attribute {
        // all uuids used by GATT are 16 bit UUIDs (except for Characteristic Value Declaration for which the value internal_16bit_uuid or internal_128bit_uuid are used)
        std::uint16_t       uuid;
        attribute_access    access;
    };
}
}

#endif