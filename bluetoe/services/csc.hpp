#ifndef BLUETOE_SERVICES_CSC_HPP
#define BLUETOE_SERVICES_CSC_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/server.hpp>
#include <bluetoe/sensor_location.hpp>
#include <bluetoe/options.hpp>

namespace bluetoe {

    namespace csc {

        struct wheel_revolution_data_supported {};
        struct crank_revolution_data_supported {};

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

            template < typename SensorList >
            using static_sensorlocation = characteristic<
                characteristic_uuid16< 0x2A5D >,
                characteristic_name< sensor_location_name >,
                fixed_uint8_value< 0x00 > /// TODO: Fill in the used sensor position
            >;

            template < typename ... Options >
            struct calculate_service {

                using sensor_locations = typename bluetoe::details::find_all_by_meta_type<
                    bluetoe::details::sensor_location_meta_type, Options... >::type;

                static constexpr bool has_static_sensorlocation = std::tuple_size< sensor_locations >::value == 1u;

                using mandatory_characteristics = std::tuple<
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
                    >
                >;

                using sensor_location_characteristics = typename bluetoe::details::select_type<
                    has_static_sensorlocation,
                    static_sensorlocation< sensor_locations >,
                    std::tuple<>
                >::type;

                using characteristics_with_sensorlocation = typename bluetoe::details::add_type<
                    mandatory_characteristics,
                    sensor_location_characteristics >::type;

                using characteristics_with_control_point = typename bluetoe::details::add_type<
                    characteristics_with_sensorlocation,
                    characteristic<
                        characteristic_uuid16< 0x2A55 >,
                        characteristic_name< control_point_name >,
                        bluetoe::no_read_access,
                        bluetoe::indicate,
                        bluetoe::mixin_write_handler< implementation, &implementation::csc_write_control_point >,
                        bluetoe::mixin_read_handler< implementation, &implementation::csc_read_control_point >
                    > >::type;

                using all_characteristics = characteristics_with_control_point;

                using default_parameter = std::tuple<
                    mixin< implementation >,
                    service_uuid,
                    bluetoe::service_name< service_name >,
                    Options... >;


                typedef typename service_from_parameters<
                    typename bluetoe::details::add_type< default_parameter, all_characteristics >::type
                 >::type type;
            };
            /** @endcond */
        }

    }

    template < typename ... Options >
    using cycling_speed_and_cadence = typename csc::details::calculate_service< Options... >::type;

    /**
     * @brief Prototype for an interface type
     */
    class cycling_speed_and_cadence_interface_prototype
    {
    protected:
        /**
         * Either this function must be provided, or cumulative_crank_revolutions(), or both.
         *
         * The function have to return the Cumulative Wheel Revolutions (first) and the
         * Last Wheel Event Time (in 1/1024s).
         *
         * Add csc::wheel_revolution_data_supported to cycling_speed_and_cadence to indicate support for wheel revolutions.
         *
         * @sa csc::wheel_revolution_data_supported
         * @sa cycling_speed_and_cadence
         */
        std::pair< std::uint32_t, std::uint16_t > cumulative_wheel_revolutions();

        /**
         * Either this function must be provided, or cumulative_wheel_revolutions(), or both.
         *
         * The function have to return the Cumulative Crank Revolutions (first) and the
         * Last Wheel Event Time (in 1/1024s)
         *
         * Add csc::crank_revolution_data_supported to cycling_speed_and_cadence to indicate support for crank revolutions.
         *
         * @sa csc::crank_revolution_data_supported
         * @sa cycling_speed_and_cadence
         */
        std::pair< std::uint16_t, std::uint16_t > cumulative_crank_revolutions();
    };
}

#endif // include guard
