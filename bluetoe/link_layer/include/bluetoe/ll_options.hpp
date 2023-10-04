#ifndef BLUETOE_LINK_LAYER_OPTIONS_HPP
#define BLUETOE_LINK_LAYER_OPTIONS_HPP

#include <bluetoe/address.hpp>
#include <bluetoe/connection_details.hpp>
#include <bluetoe/ll_meta_types.hpp>


namespace bluetoe
{
namespace link_layer
{
    namespace details {
        struct device_address_meta_type {};
        struct buffer_sizes_meta_type {};
        struct desired_connection_parameters_meta_type {};
        struct custom_l2cap_layer_meta_type {};
        struct ll_pdu_receive_data_callback_meta_type {};
    }

    /**
     * @brief defines that the device will use a static random address.
     *
     * A static random address is an address that is random, but stays the same every time the device starts.
     * A static random address is the default.
     */
    struct random_static_address
    {
        /**
         * @brief returns true, because this is a random address
         */
        static constexpr bool is_random()
        {
            return true;
        }

        /**
         * @brief takes a scheduled radio and generates a random static address
         */
        template < class Radio >
        static random_device_address address( const Radio& r )
        {
            return address::generate_static_random_address( r.static_random_address_seed() );
        }

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::device_address_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief defines a defined static random address
     *
     * The address will be A:B:C:D:E:F
     */
    template < std::uint8_t A, std::uint8_t B, std::uint8_t C, std::uint8_t D, std::uint8_t E, std::uint8_t F >
    struct static_address
    {
        /**
         * @brief returns true, because this is a random address
         */
        static constexpr bool is_random()
        {
            return true;
        }

        /**
         * @brief returns the static, configured address A:B:C:D:E:F
         */
        template < class Radio >
        static random_device_address address( const Radio& )
        {
            static constexpr std::uint8_t addr[] = { F, E, D, C, B, A };
            return ::bluetoe::link_layer::random_device_address( addr );
        }

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::device_address_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    namespace details {
        struct sleep_clock_accuracy_meta_type {};

        template < unsigned long long SleepClockAccuracyPPM >
        struct check_sleep_clock_accuracy_ppm {
            static_assert( SleepClockAccuracyPPM <= 500, "The highest, possible sleep clock accuracy is 500ppm." );

            typedef void type;
        };

    }

    /**
     * @brief defines the sleep clock accuracy of the device hardware.
     *
     * The stack uses the accuracy information to keep the time window where the peripheral listens for radio messages
     * from the central, as small as possible. It's important to determine the real sleep clock accuracy.
     * Giving to large accuracy will leed to not optimal power consumption.
     * To small accuracy will leed to instable connections.
     */
    template < unsigned long long SleepClockAccuracyPPM, typename = typename details::check_sleep_clock_accuracy_ppm< SleepClockAccuracyPPM >::type >
    struct sleep_clock_accuracy_ppm
    {
        /**
         * @brief configured sleep clock accuracy
         */
        static constexpr unsigned accuracy_ppm = static_cast< unsigned >( SleepClockAccuracyPPM );

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::sleep_clock_accuracy_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief defines link layer transmit and receive buffer sizes
     */
    template < std::size_t TransmitSize = 61, std::size_t ReceiveSize = 61 >
    struct buffer_sizes
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::buffer_sizes_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */

        /**
         * configured link layer transmit buffer size in bytes.
         */
        static constexpr std::size_t transmit_buffer_size = TransmitSize;

        /**
         * configured link layer receive buffer size in bytes.
         */
        static constexpr std::size_t receive_buffer_size  = ReceiveSize;
    };

    namespace details
    {
        struct requested_connection_parameters
        {
            std::uint16_t min_interval;
            std::uint16_t max_interval;
            std::uint16_t latency;
            std::uint16_t timeout;
        };

        struct desired_connection_parameters_base
        {
            static constexpr std::size_t    size                        = 24;
            static constexpr std::uint8_t   ll_control_pdu_code         = 3;
            static constexpr std::uint8_t   LL_CONNECTION_PARAM_REQ     = 0x0F;
            static constexpr std::uint8_t   LL_CONNECTION_PARAM_RSP     = 0x10;
            static constexpr std::uint8_t   LL_REJECT_EXT_IND           = 0x11;
            static constexpr std::uint8_t   invalid_ll_paramerters      = 0x1E;
            static constexpr std::uint8_t   unacceptable_connection_parameters = 0x3B;

            template < class Layout >
            static bool parse_and_check_params( const write_buffer& request, read_buffer response, requested_connection_parameters& params )
            {
                static constexpr std::uint16_t interval_minimum = 5u;
                static constexpr std::uint16_t interval_maximum = 3200u;
                static constexpr std::uint16_t latency_maximum  = 499u;

                const std::uint8_t* const body       = Layout::body( request ).first;

                using bluetoe::details::read_16bit;

                params.min_interval = read_16bit( body + 1 );
                params.max_interval = read_16bit( body + 3 );
                params.latency      = read_16bit( body + 5 );
                params.timeout      = read_16bit( body + 7 );

                if ( params.max_interval < params.min_interval
                  || params.min_interval < interval_minimum
                  || params.max_interval > interval_maximum
                  || params.latency > latency_maximum )
                {
                    fill< Layout >( response, {
                        ll_control_pdu_code, 3,
                        LL_REJECT_EXT_IND,
                        LL_CONNECTION_PARAM_REQ,
                        invalid_ll_paramerters
                    } );

                    return false;
                }

                return true;
            }
        };
    }

    /**
     * @brief static, desired connection parameters
     *
     * Connection parameters that the peripheral desires to have established.
     * This feature is experimental and but now, it will just fill the given
     * parameters in the LL_CONNECTION_PARAM_RSP response.
     *
     * Finally, Bluetoe should implement some mean / algorithm / heuristic to
     * establish the desired connection parameters. (for example, by requesting
     * them from the Central).
     *
     * @sa asynchronous_connection_parameter_request
     */
    template <
        std::uint16_t Interval_min,
        std::uint16_t Interval_max,
        std::uint16_t Latency_min,
        std::uint16_t Latency_max,
        std::uint16_t Timeout_min,
        std::uint16_t Timeout_max >
    struct desired_connection_parameters : private details::desired_connection_parameters_base
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::desired_connection_parameters_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < class Layout >
        bool handle_connection_parameters_request(
            const write_buffer& request, read_buffer response, const connection_details& )
        {
            const std::uint8_t* const body       = Layout::body( request ).first;
                  std::uint8_t* const write_body = Layout::body( response ).first;

            details::requested_connection_parameters params;
            if ( parse_and_check_params< Layout >( request, response, params ) )
            {
                params.min_interval = std::max( params.min_interval, Interval_min );
                params.max_interval = std::min( params.max_interval, Interval_max );

                if ( params.min_interval > params.max_interval )
                {
                    params.min_interval = Interval_min;
                    params.max_interval = Interval_max;
                }

                if ( params.latency < Latency_min || params.latency > Latency_max )
                {
                    params.latency = ( Latency_min + Latency_max ) / 2;
                }

                if ( params.timeout < Timeout_min || params.timeout > Timeout_max )
                {
                    params.timeout = ( Timeout_min + Timeout_max ) / 2;
                }

                fill< Layout >( response, {
                    ll_control_pdu_code, size,
                    LL_CONNECTION_PARAM_RSP,
                    static_cast< std::uint8_t >( params.min_interval ),
                    static_cast< std::uint8_t >( params.min_interval >> 8 ),
                    static_cast< std::uint8_t >( params.max_interval ),
                    static_cast< std::uint8_t >( params.max_interval >> 8 ),
                    static_cast< std::uint8_t >( params.latency ),
                    static_cast< std::uint8_t >( params.latency >> 8 ),
                    static_cast< std::uint8_t >( params.timeout ),
                    static_cast< std::uint8_t >( params.timeout >> 8 )
                } );

                std::copy( &body[ 9 ], &body[ size ], &write_body[ 9 ] );
            }

            return true;
        }

        template < class Layout >
        void connection_parameters_response_fill( read_buffer )
        {}

        bool connection_parameters_response_pending() const
        {
            return false;
        }

        void reset_connection_parameter_request()
        {}
        /** @endcond */
    };

    /**
     * @brief configures the link_layer to implement the connection parameter update request / response
     *        by upcalling a callback and then awaiting an response from the application.
     *
     * @sa desired_connection_parameters
     */
    template < class Callback, Callback& Obj >
    struct asynchronous_connection_parameter_request : private details::desired_connection_parameters_base
    {
    public:
        void connection_parameters_request_reply(
            std::uint16_t interval_min,
            std::uint16_t interval_max,
            std::uint16_t latency,
            std::uint16_t timeout )
        {
            pending_ = true;
            negative_ = false;
            interval_min_ = interval_min;
            interval_max_ = interval_max;
            latency_      = latency;
            timeout_      = timeout;
        }

        void connection_parameters_request_negative_reply( std::uint8_t reason )
        {
            pending_ = true;
            negative_ = true;
            reason_ = reason;
        }

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::desired_connection_parameters_meta_type,
            details::valid_link_layer_option_meta_type {};

    protected:
        template < class Layout >
        bool handle_connection_parameters_request(
            const write_buffer& request, read_buffer response, const connection_details& details )
        {
            details::requested_connection_parameters params;
            if ( !parse_and_check_params< Layout >( request, response, params ) )
                return true;

            if ( params.min_interval == params.max_interval
              && params.min_interval == details.interval()
              && params.latency == details.latency()
              && params.timeout == details.timeout() )
            {
                const std::uint8_t* const body       = Layout::body( request ).first;
                      std::uint8_t* const write_body = Layout::body( response ).first;

                fill< Layout >( response, { ll_control_pdu_code, size, LL_CONNECTION_PARAM_RSP } );

                std::copy( &body[ 1 ], &body[ 1 + size - 1 ], &write_body[ 1 ] );

                return true;
            }

            Obj.ll_remote_connection_parameter_request(
                params.min_interval, params.max_interval,
                params.latency, params.timeout );

            return false;
        }

        template < class Layout >
        void connection_parameters_response_fill( read_buffer response )
        {
            if ( negative_ )
            {
                fill< Layout >( response, {
                    ll_control_pdu_code, 3,
                    LL_REJECT_EXT_IND,
                    LL_CONNECTION_PARAM_REQ,
                    reason_
                } );
            }
            else
            {
                fill< Layout >( response, {
                    ll_control_pdu_code, size,
                    LL_CONNECTION_PARAM_RSP,
                    static_cast< std::uint8_t >( interval_min_ ),
                    static_cast< std::uint8_t >( interval_min_ >> 8 ),
                    static_cast< std::uint8_t >( interval_max_ ),
                    static_cast< std::uint8_t >( interval_max_ >> 8 ),
                    static_cast< std::uint8_t >( latency_ ),
                    static_cast< std::uint8_t >( latency_ >> 8 ),
                    static_cast< std::uint8_t >( timeout_ ),
                    static_cast< std::uint8_t >( timeout_ >> 8 ),
                    0,
                    0xff, 0xff,
                    0xff, 0xff,
                    0xff, 0xff,
                    0xff, 0xff,
                    0xff, 0xff,
                    0xff, 0xff,
                    0xff, 0xff,
                } );
            }


            pending_ = false;
        }

        bool connection_parameters_response_pending() const
        {
            return pending_;
        }

        void reset_connection_parameter_request()
        {
            pending_ = false;
        }

    private:
        bool pending_;
        bool negative_;
        std::uint16_t interval_min_;
        std::uint16_t interval_max_;
        std::uint16_t latency_;
        std::uint16_t timeout_;
        std::uint8_t  reason_;
        /** @endcond */
    };

    /** @cond HIDDEN_SYMBOLS */
    struct no_desired_connection_parameters : private details::desired_connection_parameters_base
    {
        struct meta_type :
            details::desired_connection_parameters_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < class Layout >
        bool handle_connection_parameters_request(
            const write_buffer& request, read_buffer response, const connection_details& )
        {
            details::requested_connection_parameters params;
            if ( parse_and_check_params< Layout >( request, response, params ) )
            {
                const std::uint8_t* const body       = Layout::body( request ).first;
                      std::uint8_t* const write_body = Layout::body( response ).first;

                fill< Layout >( response, { ll_control_pdu_code, size, LL_CONNECTION_PARAM_RSP } );

                std::copy( &body[ 1 ], &body[ 1 + size - 1 ], &write_body[ 1 ] );
            }

            return true;
        }

        template < class Layout >
        void connection_parameters_response_fill( read_buffer )
        {}

        bool connection_parameters_response_pending() const
        {
            return false;
        }

        void reset_connection_parameter_request()
        {}

    };
    /** @endcond */

    /**
     * @brief specify the l2cap layer to be used by the link_layer
     */
    template < template <class LinkLayer > class L2CapLayer >
    struct ll_custom_l2cap_layer
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::custom_l2cap_layer_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < class LinkLayer >
        using l2cap_layer = L2CapLayer< LinkLayer >;
        /** @endcond */
    };

    template < class Obj, Obj& obj >
    struct l2cap_callback
    {
        /** @cond HIDDEN_SYMBOLS */
        void pdu_receive_data_callback( const write_buffer& ll_pdu_containing_l2cap_package )
        {
            obj.l2cap_callback( ll_pdu_containing_l2cap_package );
        }

        struct meta_type :
            details::ll_pdu_receive_data_callback_meta_type,
            details::valid_link_layer_option_meta_type {};
    };

    struct no_l2cap_callback
    {
        /** @cond HIDDEN_SYMBOLS */
        void pdu_receive_data_callback( const write_buffer& )
        {
        }

        struct meta_type :
            details::ll_pdu_receive_data_callback_meta_type,
            details::valid_link_layer_option_meta_type {};
    };


}
}

#endif
