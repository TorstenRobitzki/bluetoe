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
            /** @cond HIDDEN_SYMBOLS */
            struct handler_tag;

            struct wheel_revolution_data_handler_tag {};
            struct crank_revolution_data_handler_tag {};
            /** @endcond */
        }

        /**
         * @brief parameter that adds, how the CSC service implementation gets the data needed.
         *
         * For further details on the requirement that Handler have to fulfile see cycling_speed_and_cadence_handler_prototype
         */
        template < typename Handler >
        struct handler
        {
            /** @cond HIDDEN_SYMBOLS */
            typedef details::handler_tag meta_type;

            typedef Handler user_handler;
            /** @endcond */
        };

        /**
         * @brief Denotes, that the csc service provides wheel revolution data.
         *
         * If added to bluetoe::cycling_speed_and_cadence, the user handler have to have the following two functions:
         * - std::pair< std::uint32_t, std::uint16_t > cumulative_wheel_revolutions_and_time()
         * - void set_cumulative_wheel_revolutions( std::uint32_t new_value )
         *
         * The first is a callback that gives the service the information about the last time, a wheel revolution
         * value was measured and the coresponding wheel revolution value.
         * The second function will be called by the service, to set the current wheel revolution value.
         *
         * A service must provide wheel_revolution_data_supported, or crank_revolution_data_supported.
         *
         * @sa crank_revolution_data_supported
         * @sa handler
         * @sa cycling_speed_and_cadence
         */
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
            struct service_from_parameters< bluetoe::details::type_list< Ts... > > {
                typedef service< Ts... > type;
            };

            struct no_wheel_revolution_data_supported {
                typedef details::wheel_revolution_data_handler_tag meta_type;

                template < class T >
                void add_wheel_mesurement( std::uint8_t&, std::uint8_t*&, T& ) {}

                static constexpr std::uint16_t features = 0;
            };

            struct no_crank_revolution_data_supported {
                typedef details::crank_revolution_data_handler_tag meta_type;

                template < class T >
                void add_crank_mesurement( std::uint8_t&, std::uint8_t*&, T& ) {}

                static constexpr std::uint16_t features = 0;
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
                rc_op_code_not_supported    = 2,
                rc_invalid_parameter        = 3
            };

            template < typename SensorLocations >
            class sensor_position_handler;

            template < typename ... SensorLocations >
            class sensor_position_handler< bluetoe::details::type_list< SensorLocations... > >
            {
            public:
                sensor_position_handler()
                    : current_position_( positions_[ 0 ] )
                    , requested_position_( current_position_ )
                {
                }

                std::uint8_t request_supported_sensor_locations_opcode_response( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    static const std::size_t response_size = 3;
                    assert( read_size >= response_size );

                    out_buffer[ 0 ] = response_code_opcode;
                    out_buffer[ 1 ] = request_supported_sensor_locations_opcode;
                    out_buffer[ 2 ] = rc_success;

                    std::size_t max_copy = std::min( read_size - response_size, sizeof ...(SensorLocations) );

                    std::copy( &positions_[ 0 ], &positions_[ max_copy ], &out_buffer[ response_size ] );
                    out_size = response_size + max_copy;

                    return error_codes::success;
                }

                std::uint8_t update_sensor_location_opcode_response( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    static const std::size_t response_size = 3;
                    assert( read_size >= response_size );

                    out_buffer[ 0 ] = response_code_opcode;
                    out_buffer[ 1 ] = update_sensor_location_opcode;

                    const auto loc = std::find( std::begin( positions_ ), std::end( positions_ ), requested_position_ );

                    if ( loc != std::end( positions_ ) )
                    {
                        out_buffer[ 2 ] = rc_success;
                        current_position_ = *loc;
                    }
                    else
                    {
                        out_buffer[ 2 ] = rc_invalid_parameter;
                    }

                    out_size = response_size;

                    return error_codes::success;
                }

                void set_sensor_position( std::uint8_t new_sensor_position )
                {
                    requested_position_ = new_sensor_position;
                }

                std::uint8_t csc_sensor_location( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    assert( read_size > 0 );

                    *out_buffer = current_position_;
                    out_size    = 1;

                    return error_codes::success;
                }


            private:
                std::uint8_t current_position_;
                std::uint8_t requested_position_;
                static const std::uint8_t positions_[sizeof ...(SensorLocations)];
            };

            template < typename ... SensorLocations >
            const std::uint8_t sensor_position_handler< bluetoe::details::type_list< SensorLocations... > >::positions_[sizeof ...(SensorLocations)] = { SensorLocations::value... };

            class no_sensor_position_handler
            {
            public:
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

                std::uint8_t update_sensor_location_opcode_response( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    static const std::size_t response_size = 3;
                    assert( read_size >= response_size );

                    out_buffer[ 0 ] = response_code_opcode;
                    out_buffer[ 1 ] = update_sensor_location_opcode;
                    out_buffer[ 2 ] = rc_op_code_not_supported;
                    out_size = response_size;

                    return error_codes::success;
                }

                void set_sensor_position( std::uint8_t ) {}
            };

            template < class SensorPositionHandler >
            class control_point_handler : public SensorPositionHandler
            {
            public:
                control_point_handler()
                    : procedure_in_progress_( false )
                {
                }

                std::uint8_t csc_read_control_point( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    procedure_in_progress_  = false;

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
                    case update_sensor_location_opcode:
                        {
                            return this->update_sensor_location_opcode_response( read_size, out_buffer, out_size );
                        }
                        break;
                    default:
                        {
                            static const std::size_t response_size = 3;
                            assert( read_size >= response_size );

                            out_buffer[ 0 ] = response_code_opcode;
                            out_buffer[ 1 ] = current_opcode_;
                            out_buffer[ 2 ] = rc_op_code_not_supported;

                            out_size = response_size;
                        }
                    }

                    return error_codes::success;
                }

                template < class Handler >
                std::pair< std::uint8_t, bool > csc_write_control_point( std::size_t write_size, const std::uint8_t* value, Handler& handler )
                {
                    if ( write_size < 1 )
                        return std::make_pair( error_codes::invalid_handle, false );

                    if ( procedure_in_progress_ )
                        return std::make_pair( error_codes::procedure_already_in_progress, false );

                    procedure_in_progress_  = true;
                    current_opcode_         = *value;
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
                            if ( write_size != 1 )
                                return std::make_pair( error_codes::invalid_pdu, false );

                            return std::make_pair( error_codes::success, true );
                        }
                    case update_sensor_location_opcode:
                        {
                            if ( write_size != 1 + 1 )
                                return std::make_pair( error_codes::invalid_pdu, false );

                            this->set_sensor_position( *value);

                            return std::make_pair( error_codes::success, true );
                        }
                    default:
                        // according to the spec with have to response with success and then indicate an error
                        return std::make_pair( error_codes::success, true );
                    }

                }
            private:
                std::uint8_t current_opcode_;
                bool         procedure_in_progress_;
            };

            struct no_control_point_handler {
                std::uint8_t csc_read_control_point( std::size_t, std::uint8_t*, std::size_t& ) {
                    return 0;
                }

                template < class Handler >
                std::pair< std::uint8_t, bool > csc_write_control_point( std::size_t, const std::uint8_t*, Handler& ) {
                    return std::make_pair( 0, false );
                }
            };

            template < typename Base, typename WheelHandler, typename CrankHandler, typename ControlPointHandler >
            class implementation : public Base, WheelHandler, CrankHandler, public ControlPointHandler
            {
            public:
                static constexpr std::size_t max_response_size = 1 + 4 + 2 + 2 + 2;

                void csc_wheel_revolution( std::uint32_t, std::uint32_t )
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

            template < bool HasSensorlocation, bool HasMultipleSensorlocations, typename SensorList >
            struct select_sensorlocation_implementation;

            template < typename SensorList >
            struct select_sensorlocation_implementation< false, false, SensorList >
            {
                using type = bluetoe::details::type_list<>;
            };

            template < typename SensorPosition >
            struct select_sensorlocation_implementation< true, false, bluetoe::details::type_list< SensorPosition > >
            {
                using type = characteristic<
                    characteristic_uuid16< 0x2A5D >,
                    characteristic_name< sensor_location_name >,
                    fixed_uint8_value< SensorPosition::value >
                >;
            };

            template < typename SensorList >
            struct select_sensorlocation_implementation< true, true, SensorList >
            {
                using type = characteristic<
                    characteristic_uuid16< 0x2A5D >,
                    characteristic_name< sensor_location_name >,
                    bluetoe::mixin_read_handler<
                        sensor_position_handler< SensorList >,
                        &sensor_position_handler< SensorList >::csc_sensor_location
                    >
                >;
            };

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


                static constexpr bool has_static_sensorlocation   = bluetoe::details::type_list_size< sensor_locations >::value == 1u;
                static constexpr bool has_multiple_sensorlocation = bluetoe::details::type_list_size< sensor_locations >::value > 1u;
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

                using mandatory_characteristics = bluetoe::details::type_list<
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

                using characteristics_with_sensorlocation = typename bluetoe::details::add_type<
                    mandatory_characteristics,
                    typename select_sensorlocation_implementation<
                        has_sensorlocation,
                        has_multiple_sensorlocation,
                        sensor_locations
                    >::type
                >::type;

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

                using default_parameter = bluetoe::details::type_list<
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
     * @typedef cycling_speed_and_cadence
     *
     * Implementation of the Cycling Speed and Cadence Service (CSCS) 1.0
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
         *
         * The update of the value have to be confirmed by calling confirm_cumulative_wheel_revolutions().
         * Calling confirm_cumulative_wheel_revolutions() can be done synchronous, or asynchroiunous.
         * It's save to call confirm_cumulative_wheel_revolutions() from an ISR.
         */
        void set_cumulative_wheel_revolutions( std::uint32_t new_value );

    };
}

#endif // include guard
