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
    };

    enum class attribute_access_type {
        read,
        write
    };

    struct attribute_access_arguments
    {
        attribute_access_type   type;
        const std::uint8_t*     input;
        const std::size_t       input_size;
        std::uint8_t*           output;
        std::size_t             output_size;

        template < std::size_t N >
        static attribute_access_arguments read( std::uint8_t(&buffer)[N] )
        {
            return attribute_access_arguments{
                attribute_access_type::read,
                nullptr,
                0,
                &buffer[ 0 ],
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
        // all uuids used by GATT are 16 bit UUIDs
        std::uint16_t       uuid;
        attribute_access    access;
    };
}
}

#endif