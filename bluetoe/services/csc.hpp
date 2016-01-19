#ifndef BLUETOE_SERVICES_CSC_HPP
#define BLUETOE_SERVICES_CSC_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/server.hpp>

namespace bluetoe {

    namespace csc {

        struct wheel_revolution_data_supported {};
        struct crank_revolution_data_supported {};
        struct multiple_sensor_locations_supported {};

        using service_uuid = service_uuid16< 0x1816 >;

        namespace details {
            /** @cond HIDDEN_SYMBOLS */
            static constexpr char service_name[] = "Cycling Speed and Cadence";
            static constexpr char measurement_name[] = "CSC Measurement";
            static constexpr char feature_name[] = "CSC Feature";
            static constexpr char sensor_location_name[] = "Sensor Location";
            static constexpr char control_point_name[] = "SC Control Point";

            template < typename T >
            struct service_from_parameters;

            template < typename ... Ts >
            struct service_from_parameters< std::tuple< Ts... > > {
                typedef service< Ts... > type;
            };

            class implementation
            {
            public:
                void csc_wheel_revolution( std::uint32_t resolution, std::uint32_t time )
                {
                }

                std::uint8_t csc_mesurement( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    return 0;
                }

                std::uint8_t csc_sensor_location( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    return 0;
                }

                std::uint8_t csc_write_control_point( std::size_t write_size, const std::uint8_t* value )
                {
                    return 0;
                }

                std::uint8_t csc_read_control_point( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    return 0;
                }
            private:
            };

            template < typename ... Options >
            struct calculate_service {

                typedef std::tuple<
                    mixin< implementation >,
                    service_uuid,
                    bluetoe::service_name< service_name >,
                    characteristic<
                        characteristic_uuid16< 0x2A5B >,
                        characteristic_name< measurement_name >,
                        bluetoe::no_read_access,
                        bluetoe::no_write_access,
                        bluetoe::notify,
                        bluetoe::mixin_read_handler< implementation, &implementation::csc_mesurement >
                    >,
                    characteristic<
                        characteristic_uuid16< 0x2A5C >,
                        characteristic_name< feature_name >,
                        fixed_uint16_value< 0x0000 >
                    >,
                    characteristic<
                        characteristic_uuid16< 0x2A5D >,
                        characteristic_name< sensor_location_name >,
                        bluetoe::no_write_access,
                        bluetoe::mixin_read_handler< implementation, &implementation::csc_sensor_location >
                    >,
                    characteristic<
                        characteristic_uuid16< 0x2A55 >,
                        characteristic_name< control_point_name >,
                        bluetoe::no_read_access,
                        bluetoe::indicate,
                        bluetoe::mixin_write_handler< implementation, &implementation::csc_write_control_point >,
                        bluetoe::mixin_read_handler< implementation, &implementation::csc_read_control_point >
                    >,
                    Options... > service_parameters;

                typedef typename service_from_parameters< service_parameters >::type type;
            };
            /** @endcond */
        }

    }

    template < typename ... Options >
    using cycling_speed_and_cadence = typename csc::details::calculate_service< Options... >::type;
}

#endif // include guard
