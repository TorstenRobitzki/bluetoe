#ifndef BLUETOE_SERVICES_CSC_HPP
#define BLUETOE_SERVICES_CSC_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/server.hpp>
#include <bluetoe/sensor_location.hpp>
#include <bluetoe/options.hpp>

namespace bluetoe {

    namespace csc {

        /**
         * The assigned 16 bit UUID for the Cycling Speed and Cadence service
         */
        using service_uuid = service_uuid16< 0x1816 >;

        namespace details {
            struct handler_tag;

            struct wheel_revolution_data_handler_tag {};
            struct crank_revolution_data_handler_tag {};
        }

        /**
         * @brief parameter that adds, how the CSC service implementation gets the data needed.
         *
         * For further details on the requirement that Handler have to fulfile see cycling_speed_and_cadence_handler_prototype
         */
        template < typename Handler >
        struct handler
        {
            typedef details::handler_tag meta_type;

            typedef Handler user_handler;
        };

        struct wheel_revolution_data_supported {
            /** @cond HIDDEN_SYMBOLS */
            typedef details::wheel_revolution_data_handler_tag meta_type;

            template < class T >
            void add_wheel_mesurement( std::uint8_t& flags, std::uint8_t*& out_buffer, T& handler )
            {
                flags |= 0x01;

                const std::pair< std::uint32_t, std::uint16_t > revolutions = handler.cumulative_wheel_revolutions_and_time();
                out_buffer = bluetoe::details::write_32bit( out_buffer, revolutions.first );
                out_buffer = bluetoe::details::write_16bit( out_buffer, revolutions.second );
            }

            static constexpr std::uint16_t features = 0x0001;
            /** @endcond */
        };

        struct crank_revolution_data_supported {
            /** @cond HIDDEN_SYMBOLS */
            typedef details::crank_revolution_data_handler_tag meta_type;

            template < class T >
            void add_crank_mesurement( std::uint8_t& flags, std::uint8_t*& out_buffer, T& handler )
            {
                flags |= 0x02;

                const std::pair< std::uint16_t, std::uint16_t > revolutions = handler.cumulative_crank_revolutions_and_time();
                out_buffer = bluetoe::details::write_16bit( out_buffer, revolutions.first );
                out_buffer = bluetoe::details::write_16bit( out_buffer, revolutions.second );
            }

            static constexpr std::uint16_t features = 0x0002;
            /** @endcond */
        };

        namespace details {
            /** @cond HIDDEN_SYMBOLS */
            static constexpr char service_name[]            = "Cycling Speed and Cadence";
            static constexpr char measurement_name[]        = "CSC Measurement";
            static constexpr char feature_name[]            = "CSC Feature";
            static constexpr char sensor_location_name[]    = "Sensor Location";
            static constexpr char control_point_name[]      = "SC Control Point";

            using control_point_uuid = characteristic_uuid16< 0x2A55 >;

            template < typename T >
            struct service_from_parameters;

            template < typename ... Ts >
            struct service_from_parameters< std::tuple< Ts... > > {
                typedef service< Ts... > type;
            };

            struct no_wheel_revolution_data_supported {
                /** @cond HIDDEN_SYMBOLS */
                typedef details::wheel_revolution_data_handler_tag meta_type;

                template < class T >
                void add_wheel_mesurement( std::uint8_t&, std::uint8_t*&, T& ) {}

                static constexpr std::uint16_t features = 0;
                /** @endcond */
            };

            struct no_crank_revolution_data_supported {
                /** @cond HIDDEN_SYMBOLS */
                typedef details::crank_revolution_data_handler_tag meta_type;

                template < class T >
                void add_crank_mesurement( std::uint8_t&, std::uint8_t*&, T& ) {}

                static constexpr std::uint16_t features = 0;
                /** @endcond */
            };
            enum {
                set_cumulative_value_opcode                 = 1,
                start_sensor_calibration_opcode,
                update_sensor_location_opcode,
                request_supported_sensor_locations_opcode,
                response_code_opcode                        = 16
            };

            enum {
                rc_success                  = 1,
                rc_op_code_not_supported    = 2
            };

            template < typename SensorLocations >
            struct sensor_position_handler;

            template < typename ... SensorLocations >
            struct sensor_position_handler< std::tuple< SensorLocations... > > {
                std::uint8_t request_supported_sensor_locations_opcode_response( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    static const std::size_t response_size = 3;
                    assert( read_size >= response_size );

                    out_buffer[ 0 ] = response_code_opcode;
                    out_buffer[ 1 ] = request_supported_sensor_locations_opcode;
                    out_buffer[ 2 ] = rc_success;

                    std::size_t max_copy = std::min( read_size - response_size, sizeof ...(SensorLocations) );

                    std::copy( &values[ 0 ], &values[ max_copy ], &out_buffer[ response_size ] );
                    out_size = response_size + max_copy;

                    return error_codes::success;
                }

                static const std::uint8_t values[sizeof ...(SensorLocations)];
            };

            template < typename ... SensorLocations >
            const std::uint8_t sensor_position_handler< std::tuple< SensorLocations... > >::values[sizeof ...(SensorLocations)] = { SensorLocations::value... };

            struct no_sensor_position_handler {
                std::uint8_t request_supported_sensor_locations_opcode_response( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    static const std::size_t response_size = 3;
                    assert( read_size >= response_size );

                    out_buffer[ 0 ] = response_code_opcode;
                    out_buffer[ 1 ] = request_supported_sensor_locations_opcode;
                    out_buffer[ 2 ] = rc_op_code_not_supported;
                    out_size = response_size;

                    return error_codes::success;
                }
            };

            template < class SensorPositionHandler >
            class control_point_handler : private SensorPositionHandler
            {
            public:
                std::uint8_t csc_read_control_point( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    switch ( current_opcode_ )
                    {
                    case set_cumulative_value_opcode:
                        {
                            static const std::size_t response_size = 3;
                            assert( read_size >= response_size );

                            out_buffer[ 0 ] = response_code_opcode;
                            out_buffer[ 1 ] = set_cumulative_value_opcode;
                            out_buffer[ 2 ] = rc_success;

                            out_size = response_size;
                        }
                        break;
                    case request_supported_sensor_locations_opcode:
                        {
                            return this->request_supported_sensor_locations_opcode_response( read_size, out_buffer, out_size );
                        }
                        break;
                    default:
                        assert( !"confirmed a control point procedure that was not running." );
                    }

                    return error_codes::success;
                }

                template < class Handler >
                std::pair< std::uint8_t, bool > csc_write_control_point( std::size_t write_size, const std::uint8_t* value, Handler& handler )
                {
                    if ( write_size < 1 )
                        return std::make_pair( error_codes::invalid_handle, false );

                    current_opcode_ = *value;
                    ++value;

                    switch ( current_opcode_ )
                    {
                    case set_cumulative_value_opcode:
                        {
                            if ( write_size != 1 + 4 )
                                return std::make_pair( error_codes::invalid_pdu, false );

                            handler.set_cumulative_wheel_revolutions( bluetoe::details::read_32bit( value ) );
                            return std::make_pair( error_codes::success, false );
                        }
                    case request_supported_sensor_locations_opcode:
                        {
                            return std::make_pair( error_codes::success, true );
                        }
                    }

                    return std::make_pair( error_codes::request_not_supported, true );
                }
            private:
                std::uint8_t current_opcode_;
            };

            struct no_control_point_handler {
                std::uint8_t csc_read_control_point( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) {
                    return 0;
                }

                template < class Handler >
                std::pair< std::uint8_t, bool > csc_write_control_point( std::size_t write_size, const std::uint8_t* value, Handler& handler ) {
                    return std::make_pair( 0, false );
                }
            };

            template < typename Base, typename WheelHandler, typename CrankHandler, typename ControlPointHandler >
            class implementation : public Base, WheelHandler, CrankHandler, public ControlPointHandler
            {
            public:
                static constexpr std::size_t max_response_size = 1 + 4 + 2 + 2 + 2;

                void csc_wheel_revolution( std::uint32_t resolution, std::uint32_t time )
                {
                }

                std::uint8_t csc_mesurement( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    assert( read_size >= max_response_size );

                    std::uint8_t* out_ptr = out_buffer + 1;

                    *out_buffer = 0;
                    this->add_wheel_mesurement( *out_buffer, out_ptr, *this );
                    this->add_crank_mesurement( *out_buffer, out_ptr, *this );

                    out_size = out_ptr - out_buffer;

                    return error_codes::success;;
                }

                std::uint8_t csc_sensor_location( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    return 0;
                }

                std::uint8_t csc_read_control_point( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    return ControlPointHandler::csc_read_control_point( read_size, out_buffer, out_size );
                }

                std::pair< std::uint8_t, bool > csc_write_control_point( std::size_t write_size, const std::uint8_t* value )
                {
                    return ControlPointHandler::csc_write_control_point( write_size, value, *this );
                }

                template < class Server >
                void notify_timed_update( Server& server )
                {
                    server.template notify< characteristic_uuid16< 0x2A5B > >();
                }

                template < class Server >
                void confirm_cumulative_wheel_revolutions( Server& server )
                {
                    server.template indicate< control_point_uuid >();
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
            struct calculate_service
            {
                using sensor_locations = typename bluetoe::details::find_all_by_meta_type<
                    bluetoe::details::sensor_location_meta_type, Options... >::type;

                using wheel_handler    = typename bluetoe::details::find_by_meta_type< wheel_revolution_data_handler_tag, Options..., no_wheel_revolution_data_supported >::type;
                using crank_handler    = typename bluetoe::details::find_by_meta_type< crank_revolution_data_handler_tag, Options..., no_crank_revolution_data_supported >::type;

                static_assert( !std::is_same< wheel_handler, bluetoe::details::no_such_type >::value, "" );
                static_assert( !std::is_same< crank_handler, bluetoe::details::no_such_type >::value, "" );

                using service_handler = typename bluetoe::details::find_by_meta_type< handler_tag, Options ... >::type;

                static_assert( !std::is_same< service_handler, bluetoe::details::no_such_type >::value,
                    "You need to provide a bluetoe::crc::handler<> to define how the protocol can access the messured values." );


                static constexpr bool has_static_sensorlocation   = std::tuple_size< sensor_locations >::value == 1u;
                static constexpr bool has_multiple_sensorlocation = std::tuple_size< sensor_locations >::value > 1u;
                static constexpr bool has_sensorlocation          = has_static_sensorlocation || has_multiple_sensorlocation;

                static constexpr bool has_set_cumulative_value    = wheel_handler::features != 0;
                static constexpr bool has_control_point           = has_set_cumulative_value | has_multiple_sensorlocation;

                using service_implementation = implementation<
                    typename service_handler::user_handler, wheel_handler, crank_handler,
                    typename bluetoe::details::select_type<
                        has_control_point,
                        typename bluetoe::details::select_type<
                            has_multiple_sensorlocation,
                            control_point_handler< sensor_position_handler< sensor_locations > >,
                            control_point_handler< no_sensor_position_handler >
                        >::type,
                        no_control_point_handler
                    >::type
                >;

                using mandatory_characteristics = std::tuple<
                    characteristic<
                        characteristic_uuid16< 0x2A5B >,
                        characteristic_name< measurement_name >,
                        bluetoe::no_read_access,
                        bluetoe::no_write_access,
                        bluetoe::notify,
                        bluetoe::mixin_read_handler< service_implementation, &service_implementation::csc_mesurement >
                    >,
                    characteristic<
                        characteristic_uuid16< 0x2A5C >,
                        characteristic_name< feature_name >,
                        fixed_uint16_value< wheel_handler::features | crank_handler::features >
                    >
                >;

                using sensor_location_characteristics = typename bluetoe::details::select_type<
                    has_sensorlocation,                         // todo multiple sensor locations
                    static_sensorlocation< sensor_locations >,
                    std::tuple<>
                >::type;

                using characteristics_with_sensorlocation = typename bluetoe::details::add_type<
                    mandatory_characteristics,
                    sensor_location_characteristics >::type;

                using characteristics_with_control_point =
                    typename bluetoe::details::select_type<
                        has_control_point,
                        typename bluetoe::details::add_type<
                            characteristics_with_sensorlocation,
                            characteristic<
                                control_point_uuid,
                                characteristic_name< control_point_name >,
                                bluetoe::no_read_access,
                                bluetoe::indicate,
                                bluetoe::mixin_write_indication_control_point_handler< service_implementation, &service_implementation::csc_write_control_point, control_point_uuid >,
                                bluetoe::mixin_read_handler< service_implementation, &service_implementation::csc_read_control_point >
                            >
                        >::type,
                        characteristics_with_sensorlocation
                    >::type;



                using all_characteristics = characteristics_with_control_point;

                using default_parameter = std::tuple<
                    mixin< service_implementation >,
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

    /**
     *
     * @sa bluetoe::csc::wheel_revolution_data_supported
     * @sa bluetoe::csc::crank_revolution_data_supported
     * @sa bluetoe::csc::handler
     */
    template < typename ... Options >
    using cycling_speed_and_cadence = typename csc::details::calculate_service< Options... >::type;

    /**
     * @brief Prototype for an interface type
     */
    class cycling_speed_and_cadence_handler_prototype
    {
    public:
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
        std::pair< std::uint32_t, std::uint16_t > cumulative_wheel_revolutions_and_time();

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
        std::pair< std::uint16_t, std::uint16_t > cumulative_crank_revolutions_and_time();

        /**
         * Function will be called, when the cumlative wheel revolution have to be changed.
         */
        void set_cumulative_wheel_revolutions( std::uint32_t new_value );

    };
}

#endif // include guard
