#ifndef BLUETOE_LINK_LAYER_ADVERTISING_HPP
#define BLUETOE_LINK_LAYER_ADVERTISING_HPP

#include <bluetoe/meta_tools.hpp>
#include <bluetoe/default_pdu_layout.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/buffer.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/ll_meta_types.hpp>

/**
 * @file bluetoe/link_layer/include/bluetoe/advertising.hpp
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
        struct advertising_channel_map_meta_type {};

        template < unsigned long long AdvertisingIntervalMilliSeconds >
        struct check_advertising_interval_parameter {
            static_assert( AdvertisingIntervalMilliSeconds >= 20,    "the advertising interval must be greater than or equal to 20ms." );
            static_assert( AdvertisingIntervalMilliSeconds <= 10240, "the advertising interval must be smaller than or equal to 10.24s." );

            typedef void type;
        };

        struct advertising_type_base {
            static constexpr std::uint8_t   header_txaddr_field         = 0x40;
            static constexpr std::uint8_t   header_rxaddr_field         = 0x80;
            static constexpr std::size_t    advertising_pdu_header_size = 2;
            static constexpr std::uint8_t   adv_ind_pdu_type_code       = 0;
            static constexpr std::uint8_t   adv_direct_ind_pdu_type_code= 1;
            static constexpr std::uint8_t   adv_nonconn_ind_pdu_type_code= 2;
            static constexpr std::uint8_t   adv_scan_ind_pdu_type_code  = 6;
            static constexpr std::uint8_t   scan_response_pdu_type_code = 4;
            static constexpr std::size_t    address_length              = 6;
            static constexpr std::size_t    maximum_adv_request_size    = 34;

            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    max_scan_response_data_size = 31;

            static constexpr std::size_t    max_advertising_pdu_size   = max_advertising_data_size + address_length;
            static constexpr std::size_t    max_scan_response_pdu_size = max_scan_response_data_size + address_length;

            template < typename Layout >
            static bool is_valid_scan_request( const read_buffer& receive, const device_address& addr )
            {
                static constexpr std::size_t  scan_request_size = 2 * address_length;
                static constexpr std::uint8_t scan_request_code = 0x03;

                const std::uint16_t header = Layout::header( receive );

                bool result = receive.size == Layout::data_channel_pdu_memory_size( scan_request_size )
                    && ( ( header >> 8 ) & 0x3f ) == scan_request_size
                    && ( header & 0x0f ) == scan_request_code;

                if ( result )
                {
                    const auto body = Layout::body( receive );
                    result = result && std::equal( &body.begin[ address_length ], &body.begin[ 2 * address_length ], addr.begin() );
                    result = result && addr.is_random() == ( ( header & header_rxaddr_field ) != 0 );
                }

                return result;
            }

            template < typename Layout >
            static bool is_valid_connect_request( const read_buffer& receive, const device_address& addr )
            {
                static constexpr std::size_t  connect_request_size = 34;
                static constexpr std::uint8_t connect_request_code = 0x05;

                if ( receive.size != Layout::data_channel_pdu_memory_size( connect_request_size ) )
                    return false;

                const std::uint16_t header = Layout::header( receive );
                const auto          body   = Layout::body( receive ).first;

                bool result = ( ( header >> 8 ) & 0x3f ) == connect_request_size
                           && ( header & 0x0f ) == connect_request_code;

                result = result && std::equal( &body[ address_length ], &body[ 2 * address_length ], addr.begin() );
                result = result && addr.is_random() == ( ( header & header_rxaddr_field ) != 0 );

                return result;
            }

            template < typename Layout >
            static std::size_t fill_empty_scan_response_data( const device_address& addr, read_buffer adv_response_buffer )
            {
                std::uint16_t header = scan_response_pdu_type_code;

                if ( addr.is_random() )
                    header |= header_txaddr_field;

                static constexpr std::size_t empty_ad_size = 2;
                std::size_t adv_response_size = advertising_pdu_header_size + address_length + empty_ad_size;
                header |= ( adv_response_size - advertising_pdu_header_size ) << 8;

                const auto body = Layout::body( adv_response_buffer );

                std::copy( addr.begin(), addr.end(), body.first );

                // add aditional empty AD to be visible to Nordic sniffer.
                // Some stacks do not recognize the response without this empty AD.
                body.first[ adv_response_size - 2 ] = 0;
                body.first[ adv_response_size - 1 ] = 0;

                Layout::header( adv_response_buffer, header );

                return adv_response_size;
            }
        };
    }


    /**
     * @example change_advertising_example.cpp
     *
     * This example demonstrates how to change the advertising type. The link layer is configured to support
     * two different advertising types (bluetoe::link_layer::connectable_undirected_advertising and bluetoe::link_layer::connectable_directed_advertising).
     * Now, the change_advertising() function can be used to switch between both advertising types.
     */

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
    class connectable_undirected_advertising
    {
    public:
        /**
         * @brief change type of advertisment
         *
         * If more than one advertising type is given, this function can be used
         * to define the advertising that is used next, when the device starts
         * advertising. If the device is currently advertising, the function
         * has no effect until the device stops advertising and starts over to
         * advertise.
         *
         * @tparam Type the next type of advertising
         */
        template < typename Type >
        void change_advertising();

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_type_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < typename LinkLayer, typename Advertiser >
        class impl : protected details::advertising_type_base
        {
        protected:
            read_buffer fill_advertising_data()
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;
                const device_address& addr = link_layer().local_address();

                // prevent assert() in layout_t::body
                adv_size_ = address_length;

                const auto buffer = advertiser().advertising_buffer();
                std::uint16_t header = adv_ind_pdu_type_code;
                std::uint8_t* body   = layout_t::body( buffer ).first;

                if ( addr.is_random() )
                    header |= header_txaddr_field;

                const std::size_t size =
                    address_length
                  + link_layer().fill_l2cap_advertising_data( &body[ address_length ], max_advertising_data_size );

                header   |= size << 8;
                adv_size_ = layout_t::data_channel_pdu_memory_size( size );

                std::copy( addr.begin(), addr.end(), body );
                layout_t::header( buffer, header );

                fill_scan_response_data();
                return buffer;
            }

            read_buffer get_advertising_data()
            {
                return advertiser().advertising_buffer();
            }

            read_buffer get_scan_response_data()
            {
                return advertiser().scan_response_buffer();
            }

            bool is_valid_scan_request( const read_buffer& receive ) const
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;
                return details::advertising_type_base::is_valid_scan_request< layout_t >( receive, link_layer().local_address() );
            }

            bool is_valid_connect_request( const read_buffer& receive ) const
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;
                return details::advertising_type_base::is_valid_connect_request< layout_t >( receive, link_layer().local_address() );
            }

        private:

            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + address_length;

            void fill_scan_response_data()
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;
                const device_address& addr = link_layer().local_address();

                const auto buffer = advertiser().scan_response_buffer();

                std::uint16_t header = scan_response_pdu_type_code;
                std::uint8_t* body   = layout_t::body( buffer ).first;

                if ( addr.is_random() )
                    header |= header_txaddr_field;

                const std::size_t size =
                    address_length
                  + link_layer().fill_l2cap_scan_response_data( &body[ address_length ], max_scan_response_data_size );

                header   |= size << 8;
                adv_response_size_ = layout_t::data_channel_pdu_memory_size( size );

                std::copy( addr.begin(), addr.end(), body );
                layout_t::header( buffer, header );
            }

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

            const LinkLayer& link_layer() const
            {
                return static_cast< const LinkLayer& >( *this );
            }

            Advertiser& advertiser()
            {
                return static_cast< Advertiser& >( *this );
            }

            std::size_t                     adv_size_;
            std::size_t                     adv_response_size_;

        };
        /** @endcond */
    };

    /**
     * @brief enables low duty connectable directed advertising
     *
     * When using directed advertising, advertising starts, when
     * no connection is established and and the directed_advertising_address()
     * function was called.
     *
     * @sa connectable_undirected_advertising
     * @sa scannable_undirected_advertising
     * @sa non_connectable_undirected_advertising
     */
    class connectable_directed_advertising
    {
    public:
        /**
         * @brief change type of advertisment
         *
         * If more than one advertising type is given, this function can be used
         * to define the advertising that is used next, when the device starts
         * advertising. If the device is currently advertising, the function
         * has no effect until the device stops advertising and starts over to
         * advertise.
         *
         * @tparam Type the next type of advertising
         */
        template < typename Type >
        void change_advertising();

        /**
         * @brief sets the address to be used in the advertising
         *        PDU.
         *
         * Starts advertising if an address was not set before.
         */
        void directed_advertising_address( const device_address& addr );

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_type_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < typename LinkLayer, typename Advertiser >
        class impl : protected details::advertising_type_base
        {
        public:
            void directed_advertising_address( const device_address& addr )
            {
                bool address_valid     = addr != device_address();
                bool start_advertising = !addr_valid_ && address_valid;

                addr_       = addr;
                addr_valid_ = address_valid;

                if ( start_advertising && started_ )
                    advertiser().handle_start_advertising();
            }

        protected:
            impl()
                : addr_valid_( false )
                , started_( false )
            {
            }

            read_buffer fill_advertising_data()
            {
                if ( !addr_valid_ )
                {
                    started_ = true;
                    return read_buffer{ nullptr, 0 };
                }

                const device_address& addr = link_layer().local_address();
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                const auto buffer = advertiser().advertising_buffer();

                std::uint8_t* const adv_data = buffer.buffer;
                std::uint16_t header = adv_direct_ind_pdu_type_code;

                if ( addr.is_random() )
                    header |= header_txaddr_field;

                if ( addr_.is_random() )
                    header |= header_rxaddr_field;

                header |= ( 2 * address_length ) << 8;

                layout_t::header( adv_data, header );

                const auto body = layout_t::body( buffer );
                std::copy( addr.begin(), addr.end(), &body.first[ 0 ] );
                std::copy( addr_.begin(), addr_.end(), &body.first[ address_length ] );

                return buffer;
            }

            read_buffer get_advertising_data()
            {
                return addr_valid_ ? advertiser().advertising_buffer() : read_buffer{ nullptr, 0 };
            }

            read_buffer get_scan_response_data() const
            {
                return read_buffer{ nullptr, 0 };
            }

            bool is_valid_scan_request( const read_buffer& ) const
            {
                return false;
            }

            bool is_valid_connect_request( const read_buffer& receive ) const
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                bool result = details::advertising_type_base::is_valid_connect_request< layout_t >( receive, link_layer().local_address() );
                const auto body   = layout_t::body( receive ).first;
                const auto header = layout_t::header( receive );

                result = result && std::equal( &body[ 0 ], &body[ address_length ], addr_.begin() ) && addr_valid_;
                result = result && addr_.is_random() == ( ( header & header_txaddr_field ) != 0 );

                return result;
            }

        private:

            static constexpr std::size_t    max_advertising_data_size   = 2 * address_length;

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

            const LinkLayer& link_layer() const
            {
                return static_cast< const LinkLayer& >( *this );
            }

            Advertiser& advertiser()
            {
                return static_cast< Advertiser& >( *this );
            }

            device_address  addr_;
            bool            addr_valid_;
            bool            started_;
        };
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
        /**
         * @brief change type of advertisment
         *
         * If more than one advertising type is given, this function can be used
         * to define the advertising that is used next, when the device starts
         * advertising. If the device is currently advertising, the function
         * has no effect until the device stops advertising and starts over to
         * advertise.
         *
         * @tparam Type the next type of advertising
         */
        template < typename Type >
        void change_advertising();

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_type_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < typename LinkLayer, typename Advertiser >
        class impl : protected details::advertising_type_base
        {
        protected:

            read_buffer fill_advertising_data()
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                fill_scan_response_data();
                const device_address& addr = link_layer().local_address();

                const auto buffer = advertiser().advertising_buffer();

                std::uint16_t header = adv_scan_ind_pdu_type_code;
                std::uint8_t* body   = layout_t::body( buffer ).first;

                if ( addr.is_random() )
                    header |= header_txaddr_field;

                const std::size_t size =
                    address_length
                  + link_layer().fill_l2cap_advertising_data( &body[ address_length ], max_advertising_data_size );

                header |= size << 8;
                adv_size_ = layout_t::data_channel_pdu_memory_size( size );

                layout_t::header( buffer, header );
                std::copy( addr.begin(), addr.end(), body );

                return buffer;
            }

            read_buffer get_advertising_data()
            {
                return read_buffer{ advertiser().advertising_buffer().buffer, adv_size_ };
            }

            read_buffer get_scan_response_data()
            {
                return read_buffer{ advertiser().scan_response_buffer().buffer, adv_response_size_ };
            }

            bool is_valid_scan_request( const read_buffer& receive ) const
            {
                return details::advertising_type_base::is_valid_scan_request( receive, link_layer().local_address() );
            }

            bool is_valid_connect_request( const read_buffer& ) const
            {
                return false;
            }

        private:
            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + address_length;

            void fill_scan_response_data()
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                adv_response_size_ = fill_empty_scan_response_data< layout_t >(
                    link_layer().local_address(), advertiser().scan_response_buffer() );
            }

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

            const LinkLayer& link_layer() const
            {
                return static_cast< const LinkLayer& >( *this );
            }

            Advertiser& advertiser()
            {
                return static_cast< Advertiser& >( *this );
            }

            std::size_t                     adv_size_;
            std::size_t                     adv_response_size_;
        };
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
        /**
         * @brief change type of advertisment
         *
         * If more than one advertising type is given, this function can be used
         * to define the advertising that is used next, when the device starts
         * advertising. If the device is currently advertising, the function
         * has no effect until the device stops advertising and starts over to
         * advertise.
         *
         * @tparam Type the next type of advertising
         */
        template < typename Type >
        void change_advertising();

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_type_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < typename LinkLayer, typename Advertiser >
        class impl : protected details::advertising_type_base
        {
        protected:
            read_buffer fill_advertising_data()
            {
                const device_address& addr = link_layer().local_address();
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                const auto buffer = advertiser().advertising_buffer();

                std::uint16_t header = adv_nonconn_ind_pdu_type_code;
                std::uint8_t* body   = layout_t::body( buffer ).first;

                if ( addr.is_random() )
                    header |= header_txaddr_field;

                const std::size_t size =
                    address_length
                  + link_layer().fill_l2cap_advertising_data( &body[ address_length ], max_advertising_data_size );

                header |= size << 8;

                adv_size_ = layout_t::data_channel_pdu_memory_size( size );

                layout_t::header( buffer, header );
                std::copy( addr.begin(), addr.end(), body );

                return buffer;
            }

            read_buffer get_advertising_data()
            {
                return advertiser().advertising_buffer();
            }

            read_buffer get_scan_response_data() const
            {
                return read_buffer{ nullptr, 0 };
            }

            bool is_valid_scan_request( const read_buffer& ) const
            {
                return false;
            }

            bool is_valid_connect_request( const read_buffer& ) const
            {
                return false;
            }

        private:
            static constexpr std::size_t    max_advertising_data_size   = 31;

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

            Advertiser& advertiser()
            {
                return static_cast< Advertiser& >( *this );
            }

            std::size_t                     adv_size_;
        };
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
    class auto_start_advertising
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_startup_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < typename Advertiser >
        struct impl
        {
            bool begin_of_advertising_events() const
            {
                return true;
            }

            bool continued_advertising_events() const
            {
                return true;
            }

            void end_of_advertising_events()
            {
            }
        };
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
    class no_auto_start_advertising
    {
    public:
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

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_startup_meta_type,
            details::valid_link_layer_option_meta_type {};

        template < typename Advertiser >
        class impl
        {
        public:
            impl()
                : started_( false )
                , enabled_( false )
                , count_( 0 )
            {}

            void start_advertising()
            {
                const bool start = !enabled_;

                count_   = 0;
                enabled_ = true;

                if ( start && started_ )
                    static_cast< Advertiser& >( *this ).handle_start_advertising();
            }

            /**
             * @brief same as start_advertising(), but the link layer will automatically stop
             *        to advertise after count advertising PDUs send.
             */
            void start_advertising( unsigned count )
            {
                assert( count );

                const bool start = !enabled_;

                count_ = count;
                enabled_ = true;

                if ( start && started_ )
                    static_cast< Advertiser& >( *this ).handle_start_advertising();
            }

            /**
             * @brief stop advertising
             */
            void stop_advertising()
            {
                enabled_ = false;
                count_   = 0;
            }

        protected:
            bool begin_of_advertising_events()
            {
                bool result = enabled_;

                if ( count_ )
                {
                    --count_;
                    if ( count_ == 0 )
                        enabled_ = false;
                }

                started_ = true;
                return result;
            }

            bool continued_advertising_events()
            {
                bool result = enabled_ && started_;

                if ( count_ )
                {
                    --count_;
                    if ( count_ == 0 )
                        enabled_ = false;
                }

                return result;
            }

            void end_of_advertising_events()
            {
                started_ = false;
                enabled_ = false;
            }
        private:
            volatile bool       started_;
            volatile bool       enabled_;
            volatile unsigned   count_;
        };
        /** @endcond */
    };

    /**
     * @brief advertising interval in ms in the range 20ms to 10.24s
     */
    template < std::uint16_t AdvertisingIntervalMilliSeconds, typename = typename details::check_advertising_interval_parameter< AdvertisingIntervalMilliSeconds >::type >
    struct advertising_interval
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_interval_meta_type,
            details::valid_link_layer_option_meta_type {};
        /** @endcond */

        /**
         * timeout in ms roundet to the next 0.625ms
         */
    protected:
        delta_time current_advertising_interval() const
        {
            return delta_time( AdvertisingIntervalMilliSeconds * 1000 );
        }
    };

    /**
     * @brief adds the abillity to change the advertising interval
     *
     * Using this type as an option to the link_layer, adds the documented
     * functions to the link_layer.
     */
    struct variable_advertising_interval
    {
    public:
        variable_advertising_interval()
            : interval_( delta_time::msec( 100 ) )
        {
        }

        /**
         * @brief sets the advertising interval in ms in the range 20ms to 10.24s
         */
        void advertising_interval_ms( unsigned interval_ms )
        {
            if ( interval_ms >= 20 and interval_ms <= 10240 )
                interval_ = delta_time::msec( interval_ms );
        }

        /**
         * @brief sets the advertising interval in ms in the range 20ms to 10.24s
         */
        void advertising_interval( delta_time interval )
        {
            if ( interval >= delta_time::msec( 20 ) and interval <= delta_time::msec( 10240 ) )
                interval_ = interval;
        }

        /**
         * @brief currently used advertising interval
         */
        delta_time current_advertising_interval() const
        {
            return interval_;
        }

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_interval_meta_type,
            details::valid_link_layer_option_meta_type {};
    private:
        delta_time interval_;
        /** @endcond */
    };

    namespace details {
        struct advertising_channel_map_base {
            static constexpr unsigned       first_advertising_channel   = 37;
            static constexpr unsigned       last_advertising_channel    = 39;
        };
    }

    /**
     * @brief adds the abillity to set a channel map for advertising
     *
     * Using this type as an option to the link_layer, adds the documented
     * functions to the link_layer.
     *
     * By default, all 3 channels are enabled. Setting the channel map to
     * empty is not supported.
     *
     * It is not supported to change the channel map during advertising.
     */
    struct variable_advertising_channel_map : details::advertising_channel_map_base
    {
        /**
         * @brief adds the given channel to the map
         */
        void add_channel_to_advertising_channel_map( unsigned channel )
        {
            assert( channel >= first_advertising_channel );
            assert( channel <= last_advertising_channel );

            channel -= first_advertising_channel;
            map_ = map_ | ( 1 << channel );

            current_channel_index_ = first_channel_index();
        }

        /**
         * @brief removes the channel from the map.
         *
         * Make sure to have at least one remaining channel when starting advertising.
         */
        void remove_channel_from_advertsing_channel_map( unsigned channel )
        {
            assert( channel >= first_advertising_channel );
            assert( channel <= last_advertising_channel );

            channel -= first_advertising_channel;
            map_ = map_ & ~( 1 << channel );

            if ( map_ )
                current_channel_index_ = first_channel_index();
        }

        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_channel_map_meta_type,
            details::valid_link_layer_option_meta_type {};
    protected:
        variable_advertising_channel_map()
            : current_channel_index_( 0 )
            , map_( 0x7 )
        {}

        unsigned current_channel() const
        {
            return current_channel_index_ + first_advertising_channel;
        }

        void next_channel()
        {
            assert( map_ != 0 );

            ++current_channel_index_;

            // seek next position in map that is set to 1
            for ( ; ( 1u << current_channel_index_ ) == 0 && ( 1u << current_channel_index_ ) <= map_; ++current_channel_index_ )
                ;

            if ( ( 1u << current_channel_index_ ) > map_ )
                current_channel_index_ = first_channel_index();
        }

        bool first_channel_selected() const
        {
            return current_channel_index_ == first_channel_index();
        }

    private:
        unsigned first_channel_index() const
        {
            unsigned result = 0;
            for ( ; ( map_ & ( 1 << result ) ) == 0; ++result )
                ;

            return result;
        }

        unsigned    current_channel_index_;
        unsigned    map_;
        /** @endcond */
    };

    /**
     * @brief channel map that contains all three advertising channels
     *
     * This is the default behaviour
     */
    struct all_advertising_channel_map : details::advertising_channel_map_base
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::advertising_channel_map_meta_type,
            details::valid_link_layer_option_meta_type {};

    protected:
        all_advertising_channel_map()
            : current_channel_index_( first_advertising_channel )
        {}

        unsigned current_channel() const
        {
            return current_channel_index_;
        }

        void next_channel()
        {
            current_channel_index_ = current_channel_index_ == last_advertising_channel
                ? first_advertising_channel
                : current_channel_index_ + 1;
        }

        bool first_channel_selected() const
        {
            return current_channel_index_ == this->first_advertising_channel;
        }

    private:
        unsigned    current_channel_index_;
        /** @endcond */
    };

    namespace details {
        /*
         * Type to implement the single and multiple adverting type advertisings
         */
        template < typename LinkLayer, typename Options, typename ... Advertisings >
        class advertiser;

        struct advertiser_base_base
        {
            static constexpr std::uint32_t  advertising_radio_access_address = 0x8E89BED6;
            static constexpr std::uint32_t  advertising_crc_init             = 0x555555;
        };

        template < typename LinkLayer, typename ... Options >
        class advertiser_base :
            public advertiser_base_base,
            public bluetoe::details::find_by_meta_type<
                    details::advertising_interval_meta_type,
                    Options..., advertising_interval< 100 > >::type,
            public bluetoe::details::find_by_meta_type<
                    details::advertising_channel_map_meta_type,
                    Options..., all_advertising_channel_map >::type
        {
        public:
            /*
             * link layer raw_pdu_buffer() is devided into 3 sections:
             *  - advertising_buffer()
             *  - scan_response_buffer()
             *  - advertising_receive_buffer()
             */
            read_buffer advertising_buffer()
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                return read_buffer{
                    base_link_layer().raw_pdu_buffer(),
                    layout_t::data_channel_pdu_memory_size( advertising_type_base::max_advertising_pdu_size ) };
            }

            read_buffer scan_response_buffer()
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                return read_buffer{
                    base_link_layer().raw_pdu_buffer()
                      + layout_t::data_channel_pdu_memory_size( advertising_type_base::max_advertising_pdu_size ),
                    layout_t::data_channel_pdu_memory_size( advertising_type_base::max_scan_response_pdu_size ) };
            }

            read_buffer advertising_receive_buffer()
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                return read_buffer{
                    base_link_layer().raw_pdu_buffer()
                      + layout_t::data_channel_pdu_memory_size( advertising_type_base::max_advertising_pdu_size )
                      + layout_t::data_channel_pdu_memory_size( advertising_type_base::max_scan_response_pdu_size ),
                    layout_t::data_channel_pdu_memory_size( advertising_type_base::maximum_adv_request_size ) };
            }

            static constexpr std::size_t maximum_required_advertising_buffer()
            {
                using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                return layout_t::data_channel_pdu_memory_size( advertising_type_base::max_advertising_pdu_size )
                     + layout_t::data_channel_pdu_memory_size( advertising_type_base::max_scan_response_pdu_size )
                     + layout_t::data_channel_pdu_memory_size( advertising_type_base::maximum_adv_request_size );
            };

        protected:
            advertiser_base()
                : adv_perturbation_( 0 )
            {
            }

            delta_time next_adv_event()
            {
                if ( !this->first_channel_selected() )
                    return delta_time::now();

                adv_perturbation_ = ( adv_perturbation_ + 7 ) % ( max_adv_perturbation_ + 1 );

                return this->current_advertising_interval() + delta_time::msec( adv_perturbation_ );
            }

            LinkLayer& base_link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

        private:
            static constexpr unsigned       max_adv_perturbation_ = 10;

            unsigned                        adv_perturbation_;
        };

        template < typename LinkLayer, typename Advertising, typename ... Options >
        struct start_stop_implementation :
            bluetoe::details::find_by_meta_type<
                advertising_startup_meta_type,
                Options...,
                auto_start_advertising
            >::type::template impl<
                advertiser<
                    LinkLayer,
                    std::tuple< Options... >,
                    Advertising
                >
            > {};

        /*
         * Implementation for a single advertising type
         */
        template < typename LinkLayer, typename ... Options, typename Advertising >
        class advertiser< LinkLayer, std::tuple< Options... >, std::tuple< Advertising > > :
            public Advertising::template impl< LinkLayer, advertiser< LinkLayer, std::tuple< Options... >, std::tuple< Advertising > > >,
            public advertiser_base< LinkLayer, Options... >,
            public start_stop_implementation< LinkLayer, std::tuple< Advertising >, Options... >
        {
        public:

            /*
             * Send out, first advertising
             */
            void handle_start_advertising()
            {
                const read_buffer advertising_data = this->fill_advertising_data();
                const read_buffer response_data    = this->get_scan_response_data();

                if ( !advertising_data.empty() && this->begin_of_advertising_events() )
                {
                    this->base_link_layer().set_access_address_and_crc_init(
                        this->advertising_radio_access_address,
                        this->advertising_crc_init );

                    this->base_link_layer().schedule_advertisment(
                        this->current_channel(),
                        write_buffer( advertising_data ),
                        write_buffer( response_data ),
                        delta_time::now(),
                        this->advertising_receive_buffer() );
                }
            }

            void handle_stop_advertising()
            {
                this->end_of_advertising_events();
            }

            /*
             * handling incomming PDU
             */
            bool handle_adv_receive( read_buffer receive, device_address& remote_address )
            {
                if ( this->is_valid_connect_request( receive ) )
                {
                    using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                    const std::uint8_t* const body   = layout_t::body( receive ).first;
                    const std::uint16_t       header = layout_t::header( receive );

                    remote_address = device_address( &body[ 0 ], header & 0x40 );

                    if ( this->base_link_layer().is_connection_request_in_filter( remote_address ) )
                        return true;
                }

                handle_adv_timeout();

                return false;
            }

            void handle_adv_timeout()
            {
                const read_buffer advertising_data = this->base_link_layer().l2cap_adverting_data_or_scan_response_data_changed()
                    ? this->fill_advertising_data()
                    : this->get_advertising_data();

                const read_buffer response_data    = this->get_scan_response_data();

                if ( !advertising_data.empty() && this->continued_advertising_events() )
                {
                    this->next_channel();

                    this->base_link_layer().schedule_advertisment(
                        this->current_channel(),
                        write_buffer( advertising_data ),
                        write_buffer( response_data ),
                        this->next_adv_event(),
                        this->advertising_receive_buffer() );
                }
            }

        };

        /*
         * Default
         */
        template < typename LinkLayer, typename ... Options >
        class advertiser< LinkLayer, std::tuple< Options... >, std::tuple<> > :
            public advertiser< LinkLayer, std::tuple< Options... >, std::tuple< connectable_undirected_advertising > >
        {
        };

        template < typename LinkLayer, typename Options, typename Advertiser, typename ... Types >
        class multipl_advertiser_base;

        template < typename LinkLayer, typename Options, typename Advertiser >
        class multipl_advertiser_base< LinkLayer, Options, Advertiser >
        {
        protected:
            read_buffer fill_advertising_data( unsigned )
            {
                return read_buffer{ nullptr, 0 };
            }

            read_buffer get_advertising_data( unsigned )
            {
                return read_buffer{ nullptr, 0 };
            }

            read_buffer get_scan_response_data( unsigned )
            {
                return read_buffer{ nullptr, 0 };
            }

            bool is_valid_scan_request( const read_buffer&, unsigned ) const
            {
                return false;
            }

            bool is_valid_connect_request( const read_buffer&, unsigned ) const
            {
                return false;
            }
         };

        template < typename LinkLayer, typename Options, typename Advertiser, typename Type, typename ... Types >
        class multipl_advertiser_base< LinkLayer, Options, Advertiser, Type, Types... > :
            public Type::template impl< LinkLayer, Advertiser >,
            public multipl_advertiser_base< LinkLayer, Options, Advertiser, Types... >
        {
        protected:

            read_buffer fill_advertising_data( unsigned selected )
            {
                return selected == 0
                    ? adv_type::fill_advertising_data()
                    : tail_type::fill_advertising_data( selected -1 );
            }

            read_buffer get_advertising_data( unsigned selected )
            {
                return selected == 0
                    ? adv_type::get_advertising_data()
                    : tail_type::get_advertising_data( selected -1 );
            }

            read_buffer get_scan_response_data( unsigned selected )
            {
                return selected == 0
                    ? adv_type::get_scan_response_data()
                    : tail_type::get_scan_response_data( selected -1 );
            }

            bool is_valid_scan_request( const read_buffer& b, unsigned selected ) const
            {
                return selected == 0
                    ? adv_type::is_valid_scan_request( b )
                    : tail_type::is_valid_scan_request( b, selected -1 );
            }

            bool is_valid_connect_request( const read_buffer& b, unsigned selected ) const
            {
                return selected == 0
                    ? adv_type::is_valid_connect_request( b )
                    : tail_type::is_valid_connect_request( b, selected -1 );
            }

        private:
            using adv_type  = typename Type::template impl< LinkLayer, Advertiser >;
            using tail_type = multipl_advertiser_base< LinkLayer, Options, Advertiser, Types... >;
        };

        /*
         * Wrapper around multiple advertising types
         */
        template < typename LinkLayer, typename ... Options, typename FirstAdv, typename SecondAdv, typename ... Advertisings >
        class advertiser< LinkLayer, std::tuple< Options... >, std::tuple< FirstAdv, SecondAdv, Advertisings... > > :
            public advertiser_base< LinkLayer, Options... >,
            public multipl_advertiser_base<
                LinkLayer,
                std::tuple< Options... >,
                advertiser< LinkLayer, std::tuple< Options... >, std::tuple< FirstAdv, SecondAdv, Advertisings... > >,
                FirstAdv, SecondAdv, Advertisings...
            >,
            public start_stop_implementation< LinkLayer, std::tuple< FirstAdv, SecondAdv, Advertisings... >, Options... >
        {
        public:
            advertiser()
                : selected_( 0 )
                , proposal_( 0 )
            {
            }

            void handle_start_advertising()
            {
                selected_ = proposal_;
                const read_buffer advertising_data = this->fill_advertising_data( selected_ );
                const read_buffer response_data    = this->get_scan_response_data( selected_ );

                if ( !advertising_data.empty() && this->begin_of_advertising_events() )
                {
                    this->base_link_layer().set_access_address_and_crc_init(
                        this->advertising_radio_access_address,
                        this->advertising_crc_init );

                    this->base_link_layer().schedule_advertisment(
                        this->current_channel(),
                        write_buffer( advertising_data ),
                        write_buffer( response_data ),
                        delta_time::now(),
                        this->advertising_receive_buffer() );
                }
            }

            void handle_stop_advertising()
            {
                this->end_of_advertising_events();
            }

            bool handle_adv_receive( read_buffer receive, device_address& remote_address )
            {
                if ( this->is_valid_connect_request( receive, selected_ ) )
                {
                    using layout_t = typename pdu_layout_by_radio< typename LinkLayer::radio_t >::pdu_layout;

                    const std::uint8_t* const body   = layout_t::body( receive ).first;
                    const std::uint16_t       header = layout_t::header( receive );

                    remote_address = device_address( &body[ 0 ], header & 0x40 );

                    if ( this->base_link_layer().is_connection_request_in_filter( remote_address ) )
                        return true;
                }

                handle_adv_timeout();

                return false;
            }

            void handle_adv_timeout()
            {
                const bool fill_data = selected_ != proposal_
                    || this->base_link_layer().l2cap_adverting_data_or_scan_response_data_changed();

                selected_ = proposal_;
                const read_buffer advertising_data = fill_data
                    ? this->fill_advertising_data( selected_ )
                    : this->get_advertising_data( selected_ );

                const read_buffer response_data    = this->get_scan_response_data( selected_ );

                if ( !advertising_data.empty() && this->continued_advertising_events() )
                {
                    this->next_channel();

                    this->base_link_layer().schedule_advertisment(
                        this->current_channel(),
                        write_buffer( advertising_data ),
                        write_buffer( response_data ),
                        this->next_adv_event(),
                        this->advertising_receive_buffer() );
                }
            }

            /*
             * this is the true implementation of all the documentated function within the advertising types
             */
            template < typename Type >
            void change_advertising()
            {
                proposal_ = bluetoe::details::index_of< Type, FirstAdv, SecondAdv, Advertisings... >::value;
                assert( proposal_ != sizeof...(Advertisings) + 2 );
            }

        private:

            unsigned selected_;
            unsigned proposal_;
        };

        /** @cond HIDDEN_SYMBOLS */
        template < typename LinkLayer, typename ... Options >
        using select_advertiser_implementation =
            advertiser<
                LinkLayer,
                std::tuple< Options... >,
                typename bluetoe::details::find_all_by_meta_type< advertising_type_meta_type,
                    Options... >::type >;
        /** @endcond */
    }
}
}
#endif
