#ifndef BLUETOE_DESCRIPTOR_HPP
#define BLUETOE_DESCRIPTOR_HPP

#include <bluetoe/meta_types.hpp>
#include <cstdint>
#include <cstddef>

namespace bluetoe {

    namespace details {
        struct descriptor_parameter {};
    }

    /**
     * @brief User defined descriptor
     *
     * A user defined, read only descriptor with a 16-bit UUID.
     * @note Currently, it is only supported to have one such user defined descriptor in a characteristic.
     */
    template < std::uint16_t UUID, const std::uint8_t* const Value, std::size_t Size >
    struct descriptor
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type : details::descriptor_parameter, details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };
}

#endif
