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
        struct desired_connection_parameters_base
        {
            static constexpr std::size_t    size                        = 24;
            static constexpr std::uint8_t   ll_control_pdu_code         = 3;
            static constexpr std::uint8_t   LL_CONNECTION_PARAM_RSP     = 0x10;
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
        static void fill_response(
            const write_buffer& request, read_buffer response )
        {
            const std::uint8_t* const body       = Layout::body( request ).first;
                  std::uint8_t* const write_body = Layout::body( response ).first;

            using bluetoe::details::read_16bit;

            std::uint16_t min_interval = std::max( read_16bit( body + 1 ), Interval_min );
            std::uint16_t max_interval = std::min( read_16bit( body + 3 ), Interval_max );
            std::uint16_t latency      = read_16bit( body + 5 );
            std::uint16_t timeout      = read_16bit( body + 7 );

            if ( min_interval > max_interval )
            {
                min_interval = Interval_min;
                max_interval = Interval_max;
            }

            if ( latency < Latency_min || latency > Latency_max )
            {
                latency = ( Latency_min + Latency_max ) / 2;
            }

            if ( timeout < Timeout_min || timeout > Timeout_max )
            {
                timeout = ( Timeout_min + Timeout_max ) / 2;
            }

            fill< Layout >( response, {
                ll_control_pdu_code, size,
                LL_CONNECTION_PARAM_RSP,
                static_cast< std::uint8_t >( min_interval ),
                static_cast< std::uint8_t >( min_interval >> 8 ),
                static_cast< std::uint8_t >( max_interval ),
                static_cast< std::uint8_t >( max_interval >> 8 ),
                static_cast< std::uint8_t >( latency ),
                static_cast< std::uint8_t >( latency >> 8 ),
                static_cast< std::uint8_t >( timeout ),
                static_cast< std::uint8_t >( timeout >> 8 )
            } );

            std::copy( &body[ 9 ], &body[ size ], &write_body[ 9 ] );
        }
        /** @endcond */
    };

    /** @cond HIDDEN_SYMBOLS */
    struct no_desired_connection_parameters : private details::desired_connection_parameters_base
    {
        struct meta_type :
            details::desired_connection_parameters_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < class Layout >
        static void fill_response(
            const write_buffer& request, read_buffer response )
        {
            const std::uint8_t* const body       = Layout::body( request ).first;
                  std::uint8_t* const write_body = Layout::body( response ).first;

            fill< Layout >( response, { ll_control_pdu_code, size, LL_CONNECTION_PARAM_RSP } );

            std::copy( &body[ 1 ], &body[ 1 + size - 1 ], &write_body[ 1 ] );
        }
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
