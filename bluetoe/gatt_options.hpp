#ifndef BLUETOE_GATT_OPTIONS_HPP
#define BLUETOE_GATT_OPTIONS_HPP

#include <bluetoe/meta_types.hpp>

#include <cstdint>
#include <cstddef>

namespace bluetoe {

    namespace details {
        struct mtu_size_meta_type {};
        struct cccd_callback_meta_type {};
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

    /**
     * @brief callback to be called, if the client characteristic configuration has changed
     *        or was written.
     *
     * The provided callback type T must be a class with the following public accessible function:
     *
     * @code
     * template < class Server >
     * void client_characteristic_configuration_updated( Server&, const client_characteristic_configuration& );
     * @endcode
     *
     * Where Server is the configured instance of a `bluetoe::server<>` instance.
     */
    template < typename T, T& Obj >
    struct client_characteristic_configuration_update_callback
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::cccd_callback_meta_type,
            details::valid_server_option_meta_type {};

        template < class Server >
        static void client_characteristic_configuration_updated( Server& srv, const bluetoe::details::client_characteristic_configuration& data )
        {
            Obj.client_characteristic_configuration_updated( srv, data );
        }
        /** @endcond */
    };

    /** @cond HIDDEN_SYMBOLS */
    struct no_client_characteristic_configuration_update_callback
    {
        struct meta_type :
            details::cccd_callback_meta_type,
            details::valid_server_option_meta_type {};

        template < class Server >
        static void client_characteristic_configuration_updated( Server&, const bluetoe::details::client_characteristic_configuration& )
        {
        }
    };
    /** @endcond */
}

#endif