#ifndef BLUETOE_GATT_OPTIONS_HPP
#define BLUETOE_GATT_OPTIONS_HPP

#include <bluetoe/meta_types.hpp>

#include <cstdint>
#include <cstddef>

namespace bluetoe {

    namespace details {
        struct mtu_size_meta_type {};
    }

    /**
     * @brief define the maximum GATT MTU size to be used
     *
     * The default is the minimum of 23.
     *
     * @sa server
     */
    template < std::uint16_t MaxMTU >
    struct max_mtu_size {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::mtu_size_meta_type,
            details::valid_server_option_meta_type {};

        static constexpr std::size_t mtu = MaxMTU;
        /** @endcond */
    };

}

#endif