#ifndef BLUETOE_SERVICES_CSC_HPP
#define BLUETOE_SERVICES_CSC_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/server.hpp>

namespace bluetoe {

    namespace csc {

        struct wheel_revolution_data_supported {};
        struct crank_revolution_data_supported {};
        struct multiple_sensor_locations_supported {};

        namespace details {
            static constexpr char service_name[] = "Cycling Speed and Cadence";
            static constexpr char measurement_name[] = "CSC Measurement";
            static constexpr char feature_name[] = "CSC Feature";
            static constexpr char sensor_location_name[] = "Sensor Location";
            static constexpr char control_point_name[] = "SC Control Point";
        }

        using service_uuid = service_uuid16< 0x1816 >;
    }


    uint32_t dummy;

    template < typename ... Options >
    struct cycling_speed_and_cadence :
        service<
            csc::service_uuid,
            service_name< csc::details::service_name >,
            characteristic<
                characteristic_uuid16< 0x2A5B >,
                characteristic_name< csc::details::measurement_name >,
                bluetoe::no_read_access,
                bluetoe::no_write_access,
                bluetoe::notify,
                bluetoe::bind_characteristic_value< decltype( dummy ), &dummy >
            >,
            characteristic<
                characteristic_uuid16< 0x2A5C >,
                characteristic_name< csc::details::feature_name >,
                fixed_uint16_value< 0x0000 >
            >,
            characteristic<
                characteristic_uuid16< 0x2A5D >,
                characteristic_name< csc::details::sensor_location_name >,
                bluetoe::no_write_access,
                bluetoe::bind_characteristic_value< decltype( dummy ), &dummy >
            >,
            characteristic<
                characteristic_uuid16< 0x2A55 >,
                characteristic_name< csc::details::control_point_name >,
                bluetoe::no_read_access,
                bluetoe::indicate,
                bluetoe::bind_characteristic_value< decltype( dummy ), &dummy >
            >
        >
    {
    };
}

#endif // include guard
