#ifndef BLUETOE_GAP_SERVICE_HPP
#define BLUETOE_GAP_SERVICE_HPP

#include <cstdint>
#include <bluetoe/service.hpp>
#include <bluetoe/appearance.hpp>

namespace bluetoe {

    namespace details {
        struct gap_service_definition_meta_type {};

        template < std::uint16_t MinConnectionInterval, std::uint16_t MaxConnectionInterval >
        struct check_gap_service_for_gatt_servers_parameter
        {

        };
    };

    struct preferred_connection_parameters;

    /**
     * @brief if used a parameter to a server, the GATT server will not include a GAP service for GATT servers.
     *
     * According to BLE 4.2 it's mandatory to have a GAP service for GATT service.
     * @sa server
     */
    struct no_gap_service_for_gatt_servers {
        typedef details::gap_service_definition_meta_type meta_type;

        template < typename Services, typename ... ServerOptions >
        struct add_service {
            typedef Services type;
        };
    };

    struct gap_service_for_gatt_servers
    {
        typedef details::gap_service_definition_meta_type meta_type;

        template < typename Appearance >
        using gap_service =
            service<
                service_uuid16< 0x1800 >,
                characteristic<
                    characteristic_uuid16< 0x2A00 >,
                    fixed_uint16_value< 0 > // TODO resonable default
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

            typedef typename details::add_type< Services, gap_service< device_appearance > >::type type;
        };
    };
}

#endif
