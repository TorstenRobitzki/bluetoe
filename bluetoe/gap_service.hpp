#ifndef BLUETOE_GAP_SERVICE_HPP
#define BLUETOE_GAP_SERVICE_HPP

#include <cstdint>
#include <bluetoe/service.hpp>
#include <bluetoe/appearance.hpp>
#include <bluetoe/server_name.hpp>

namespace bluetoe {

    namespace details {
        struct gap_service_definition_meta_type {};

        template < std::uint16_t MinConnectionInterval, std::uint16_t MaxConnectionInterval >
        struct check_gap_service_for_gatt_servers_parameter
        {

        };
    }

    /**
     * @brief Used as a parameter to a server, to define that the GATT server will _not_ include a GAP service for GATT servers.
     *
     * According to BLE 4.2 it's mandatory to have a GAP service for GATT service.
     * @sa server
     * @sa gap_service_for_gatt_servers
     */
    struct no_gap_service_for_gatt_servers {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::gap_service_definition_meta_type,
            details::valid_server_option_meta_type {};

        template < typename Services, typename ... ServerOptions >
        struct add_service {
            typedef Services type;
        };
        /** @endcond */
    };

    /**
     * @brief Used as a parameter to a server, to define that the GATT server will include a GAP service for GATT servers.
     *
     * This is the default. The GAP Service for GATT Servers is configured directly by passing arguments to the server definition.
     *
     * @sa server
     * @sa appearance
     * @sa no_gap_service_for_gatt_servers
     */
    struct gap_service_for_gatt_servers
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::gap_service_definition_meta_type,
            details::valid_server_option_meta_type {};

        template < typename Appearance, typename Name >
        using gap_service =
            service<
                service_uuid16< 0x1800 >,
                characteristic<
                    characteristic_uuid16< 0x2A00 >,
                    cstring_wrapper< Name >
                >,
                characteristic<
                    characteristic_uuid16< 0x2A01 >,
                    fixed_uint16_value< Appearance::value >
                >
            >;

        template < typename Services, typename ... ServerOptions >
        struct add_service {
            typedef typename details::find_by_meta_type<
                details::device_appearance_meta_type,
                ServerOptions...,
                appearance::unknown
            >::type device_appearance;

            static constexpr char default_server_name[ 15 ] = "Bluetoe-Server";

            typedef typename details::find_by_meta_type<
                details::server_name_meta_type,
                ServerOptions...,
                server_name< default_server_name >
            >::type device_name;

            typedef typename details::add_type<
                Services,
                gap_service< device_appearance, device_name >
            >::type type;
        };

        /** @endcond */
    };

    /** @cond HIDDEN_SYMBOLS */
    template < typename Services, typename ... ServerOptions >
    constexpr char gap_service_for_gatt_servers::add_service< Services, ServerOptions... >::default_server_name[ 15 ];
    /** @endcond */

}

#endif
