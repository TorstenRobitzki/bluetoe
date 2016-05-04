#ifndef BLUETOE_LINK_LAYER_OPTIONS_HPP
#define BLUETOE_LINK_LAYER_OPTIONS_HPP

#include <bluetoe/link_layer/address.hpp>
#include <bluetoe/link_layer/connection_details.hpp>


namespace bluetoe
{
namespace link_layer
{
    class connection_details;

    namespace details {
        struct advertising_interval_meta_type {};
        struct device_address_meta_type {};
        struct buffer_sizes_meta_type {};
        struct mtu_size_meta_type {};
        struct connection_callbacks_meta_type {};

        template < unsigned long long AdvertisingIntervalMilliSeconds >
        struct check_advertising_interval_parameter {
            static_assert( AdvertisingIntervalMilliSeconds >= 20,    "the advertising interval must be greater than or equal to 20ms." );
            static_assert( AdvertisingIntervalMilliSeconds <= 10240, "the advertising interval must be greater than or equal to 20ms." );

            typedef void type;
        };

    }

    /**
     * @brief advertising interval in ms in the range 20ms to 10.24s
     */
    template < std::uint16_t AdvertisingIntervalMilliSeconds, typename = typename details::check_advertising_interval_parameter< AdvertisingIntervalMilliSeconds >::type >
    struct advertising_interval
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_interval_meta_type meta_type;
        /** @endcond */

        /**
         * timeout in ms roundet to the next 0.625ms
         */
        static constexpr delta_time interval() {

            return delta_time( AdvertisingIntervalMilliSeconds * 1000 );
        }
    };

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
        static address address( const Radio& r )
        {
            return address::generate_static_random_address( r.static_random_address_seed() );
        }

        /** @cond HIDDEN_SYMBOLS */
        typedef details::device_address_meta_type meta_type;
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
        static address address( const Radio& )
        {
            static const std::uint8_t addr[] = { F, E, D, C, B, A };
            return ::bluetoe::link_layer::address( addr );
        }

        /** @cond HIDDEN_SYMBOLS */
        typedef details::device_address_meta_type meta_type;
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
     * The stack uses the accuracy information to keep the time window where the slave listens for radio messages
     * from the master, as small as possible. It's important to determine the real sleep clock accuracy.
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
        typedef details::sleep_clock_accuracy_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief defines link layer transmit and receive buffer sizes
     */
    template < std::size_t TransmitSize = 59, std::size_t ReceiveSize = 59 >
    struct buffer_sizes
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::buffer_sizes_meta_type meta_type;
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

    /**
     * @brief define the maximum L2CAP MTU size to be used by the link layer
     *
     * The default is the minimum of 23.
     */
    template < std::uint8_t MaxMTU >
    struct max_mtu_size {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::mtu_size_meta_type meta_type;

        static constexpr std::size_t mtu = MaxMTU;
        /** @endcond */
    };

    /**
     * @brief provides a type and an instance to call connection related callbacks on.
     *
     * The parameter T have to be a class type with following optional none static member
     * functions:
     *
     * template < typename ConnectionData >
     * void ll_connection_established( const bluetoe::link_layer::connection_details& details, const ConnectionData& connection );
     *
     * template < typename ConnectionData >
     * void ll_connection_changed( const bluetoe::link_layer::connection_details& details, const ConnectionData& connection );
     *
     * template < typename ConnectionData >
     * void ll_connection_closed( const ConnectionData& connection );
     */
    template < typename T, T& Obj >
    struct connection_callbacks {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::connection_callbacks_meta_type meta_type;

        template < class Server >
        class impl {
        public:
            impl()
                : event_type_( none )
                , connection_( nullptr )
            {
            }

            // this functions are called from the interrupt handlers of the scheduled radio and just store the informations that
            // are provided.
            template < class Radio >
            void connection_established( const bluetoe::link_layer::connection_details& details, const typename Server::connection_data& connection, Radio& r )
            {
                event_type_ = established;
                connection_ = &connection;
                details_    = details;

                r.wake_up();
            }

            template < class Radio >
            void connection_changed( const bluetoe::link_layer::connection_details& details, const typename Server::connection_data& connection, Radio& r )
            {
                event_type_ = changed;
                connection_ = &connection;
                details_    = details;

                r.wake_up();
            }

            template < class Radio >
            void connection_closed( const typename Server::connection_data& connection, Radio& r )
            {
                event_type_ = closed;
                connection_ = &connection;

                r.wake_up();
            }

            void handle_connection_events() {
                if ( event_type_ == established )
                {
                    call_ll_connection_established< T >( Obj, details_, *connection_ );
                }
                else if ( event_type_ == changed )
                {
                    call_ll_connection_changed< T >( Obj, details_, *connection_ );
                }
                else if ( event_type_ == closed )
                {
                    call_ll_connection_closed< T >( Obj, *connection_ );
                }

                event_type_ = none;
                connection_ = nullptr;
            }

        private:
            enum {
                none,
                established,
                changed,
                closed
            } event_type_;

            const typename Server::connection_data*     connection_;
            bluetoe::link_layer::connection_details     details_;

            template < typename TT >
            auto call_ll_connection_established( TT& obj, const bluetoe::link_layer::connection_details& details, const typename Server::connection_data& connection )
                -> decltype(&TT::template ll_connection_established< typename Server::connection_data >)
            {
                obj.ll_connection_established( details, connection );

                return 0;
            }

            template < typename TT >
            auto call_ll_connection_changed( TT& obj, const bluetoe::link_layer::connection_details& details, const typename Server::connection_data& connection )
                -> decltype(&TT::template ll_connection_changed< typename Server::connection_data >)
            {
                obj.ll_connection_changed( details, connection );

                return 0;
            }

            template < typename TT >
            auto call_ll_connection_closed( TT& obj, const typename Server::connection_data& connection )
                -> decltype(&TT::template ll_connection_closed< typename Server::connection_data >)
            {
                obj.ll_connection_closed( connection );

                return 0;
            }

            template < typename TT >
            void call_ll_connection_established( ... )
            {
            }

            template < typename TT >
            void call_ll_connection_changed( ... )
            {
            }

            template < typename TT >
            void call_ll_connection_closed( ... )
            {
            }
        };

        /** @endcond */
    };

    namespace details {
        struct no_connection_callbacks {
            typedef details::connection_callbacks_meta_type meta_type;

            template < class Server >
            struct impl {
                template < class Radio >
                void connection_established( const bluetoe::link_layer::connection_details& details, const typename Server::connection_data& connection, Radio& r ) {}

                template < class Radio >
                void connection_changed(  const bluetoe::link_layer::connection_details& details, const typename Server::connection_data& connection, Radio& r ) {}

                template < class Radio >
                void connection_closed( const typename Server::connection_data& connection, Radio& r ) {}

                void handle_connection_events() {}
            };
        };
    }

}
}

#endif
