#ifndef BLUETOE_LINK_LAYER_ADVERTISING_HPP
#define BLUETOE_LINK_LAYER_ADVERTISING_HPP

#include <bluetoe/options.hpp>

/**
 * @file bluetoe/link_layer/advertising.hpp
 *
 * Design criterias for advertising:
 * - resonable default (connectable undirected adv.)
 * - no overhead if defaults are used
 * - configurable advertising interval
 * - advertising count implementable
 */

namespace bluetoe {
namespace link_layer {

    namespace details {
        struct advertising_type_meta_type {};
        struct advertising_startup_meta_type {};
        struct advertising_interval_meta_type {};

        template < unsigned long long AdvertisingIntervalMilliSeconds >
        struct check_advertising_interval_parameter {
            static_assert( AdvertisingIntervalMilliSeconds >= 20,    "the advertising interval must be greater than or equal to 20ms." );
            static_assert( AdvertisingIntervalMilliSeconds <= 10240, "the advertising interval must be greater than or equal to 20ms." );

            typedef void type;
        };
    }

    /**
     * @brief enables connectable undirected advertising
     *
     * If no advertising type is specified, this will be the
     * default.
     *
     * @sa connectable_directed_advertising
     * @sa scannable_undirected_advertising
     * @sa non_connectable_undirected_advertising
     */
    struct connectable_undirected_advertising
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_type_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief enables low duty connectable directed advertising
     *
     * @sa connectable_undirected_advertising
     * @sa scannable_undirected_advertising
     * @sa non_connectable_undirected_advertising
     */
    struct connectable_directed_advertising
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_type_meta_type meta_type;
        /** @endcond */

    };

    /**
     * @brief enables scannable undirected advertising
     *
     * @sa connectable_undirected_advertising
     * @sa connectable_directed_advertising
     * @sa non_connectable_undirected_advertising
     */
    struct scannable_undirected_advertising
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_type_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief enables non-connectable undirected advertising
     *
     * @sa connectable_undirected_advertising
     * @sa connectable_directed_advertising
     * @sa scannable_undirected_advertising
     */
    struct non_connectable_undirected_advertising
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_type_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief if this options is given to the link layer, the link layer will start to
     *        advertise automatically, when started or when disconnected.
     *
     * This is the default behaviour.
     *
     * @sa no_auto_start_advertising
     */
    struct auto_start_advertising
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_startup_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief if this options is given to the link layer, the link layer will _not_ start to
     *        advertise automatically, when started or when disconnected.
     *
     * This option will add the following functions to the link_layer:
     * void start_advertising()
     * void stop_advertising()
     * bool is_advertising()
     *
     * @sa auto_start_advertising
     */
    struct no_auto_start_advertising
    {
        /**
         * @brief starts to advertise.
         *
         * If the device is currently connected and only one connection is supported, the
         * link layer will terminate the connection and then start to advertise.
         */
        void start_advertising();

        /**
         * @brief same as start_advertising(), but the link layer will automatically stop
         *        to advertise after count advertising events.
         */
        void start_advertising( unsigned count );

        /**
         * @brief stop advertising
         */
        void stop_advertising();

        /**
         * @brief returns if the link layer is currently advertising
         */
        bool is_advertising() const;

        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_startup_meta_type meta_type;
        /** @endcond */
    };

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
     * @brief adds the abillity to change the advertising channel
     *
     * This option adds the following functions to the link_layer:
     * - void advertising_interval_ms( unsigned interval_ms );
     * - unsigned advertising_interval_ms() const;
     */
    struct variable_advertising_interval
    {
        /**
         * @brief sets the advertising interval in ms in the range 20ms to 10.24s
         */
        void advertising_interval_ms( unsigned interval_ms );

        /**
         * @brief returns the currently used advertising interval
         */
        unsigned advertising_interval_ms() const;

        /**
         * timeout in ms roundet to the next 0.625ms
         */
        static constexpr delta_time interval() {

            return delta_time( 4 * 1000 );
        }

        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_interval_meta_type meta_type;
        /** @endcond */
    };

    namespace details {
        template < typename ... Options >
        class advertising_state_impl
        {
        public:
            advertising_state_impl()
                : current_channel_index_( first_advertising_channel )
                , adv_perturbation_( 0 )
            {
            }

            template < typename LinkLayer >
            void handle_start_advertising( LinkLayer& link_layer, std::uint8_t* advertising_receive_buffer )
            {
                current_channel_index_ = first_advertising_channel;

                link_layer.schedule_advertisment_and_receive(
                    current_channel_index_,
                    link_layer.advertising_data(),
                    delta_time::now(),
                    read_buffer{ advertising_receive_buffer, maximum_adv_request_size } );
            }

            template < typename LinkLayer >
            bool handle_non_connected_state(
                const read_buffer& receive, LinkLayer& link_layer, const device_address& addr, device_address& remote_address )
            {
                if ( is_valid_scan_request( receive, addr ) )
                {
                    remote_address = device_address( &receive.buffer[ 2 ], receive.buffer[ 0 ] & 0x40 );

                    if ( link_layer.is_scan_request_in_filter( remote_address ) )
                    {
                        link_layer.schedule_advertisment_and_receive(
                            current_channel_index_,
                            link_layer.advertising_response_data(),
                            delta_time::now(),
                            read_buffer{ nullptr, 0 } );

                        return false;
                    }
                }
                else if ( is_valid_connect_request( receive, addr ) )
                {
                    remote_address = device_address( &receive.buffer[ 2 ], receive.buffer[ 0 ] & 0x40 );
                    return true;
                }

                handle_adv_timeout( link_layer, receive.buffer );

                return false;
            }

            template < typename LinkLayer >
            void handle_adv_timeout( LinkLayer& link_layer, std::uint8_t* advertising_receive_buffer )
            {
                current_channel_index_ = current_channel_index_ == last_advertising_channel
                    ? first_advertising_channel
                    : current_channel_index_ + 1;

                const delta_time next_time = current_channel_index_ == first_advertising_channel
                    ? next_adv_event()
                    : delta_time::now();

                link_layer.schedule_advertisment_and_receive(
                    current_channel_index_,
                    link_layer.advertising_data(), next_time,
                    read_buffer{ advertising_receive_buffer, maximum_adv_request_size } );
            }

        private:
            typedef                         advertising_interval< 100 >         default_advertising_interval;

            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    advertising_pdu_header_size = 2;
            static constexpr std::size_t    address_length              = 6;
            static constexpr std::size_t    maximum_adv_request_size    = 34 + advertising_pdu_header_size;

            static constexpr unsigned       first_advertising_channel   = 37;
            static constexpr unsigned       last_advertising_channel    = 39;
            static constexpr unsigned       max_adv_perturbation_       = 10;

            unsigned                        current_channel_index_;
            unsigned                        adv_perturbation_;

            bool is_valid_scan_request( const read_buffer& receive, const device_address& addr ) const
            {
                static constexpr std::size_t  scan_request_size = 2 * address_length + advertising_pdu_header_size;
                static constexpr std::uint8_t scan_request_code = 0x03;

                bool result = receive.size == scan_request_size
                    && ( receive.buffer[ 1 ] & 0x3f ) == scan_request_size - advertising_pdu_header_size
                    && ( receive.buffer[ 0 ] & 0x0f ) == scan_request_code;

                result = result && std::equal( &receive.buffer[ 8 ], &receive.buffer[ 14 ], addr.begin() );

                return result;
            }

            bool is_valid_connect_request( const read_buffer& receive, const device_address& addr ) const
            {
                static constexpr std::size_t  connect_request_size = 34 + advertising_pdu_header_size;
                static constexpr std::uint8_t connect_request_code = 0x05;

                bool result = receive.size == connect_request_size
                        && ( receive.buffer[ 1 ] & 0x3f ) == connect_request_size - advertising_pdu_header_size
                        && ( receive.buffer[ 0 ] & 0x0f ) == connect_request_code;

                result = result && std::equal( &receive.buffer[ 8 ], &receive.buffer[ 14 ], addr.begin() );

                return result;
            }

            delta_time next_adv_event()
            {
                adv_perturbation_ = ( adv_perturbation_ + 7 ) % ( max_adv_perturbation_ + 1 );

                typedef typename ::bluetoe::details::find_by_meta_type<
                    details::advertising_interval_meta_type,
                    Options..., default_advertising_interval >::type adv_interval;

                return adv_interval::interval() + delta_time::msec( adv_perturbation_ );
            }
        };
    }
}
}
#endif
