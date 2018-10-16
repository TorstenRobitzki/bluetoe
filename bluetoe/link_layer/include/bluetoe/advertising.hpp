#ifndef BLUETOE_LINK_LAYER_ADVERTISING_HPP
#define BLUETOE_LINK_LAYER_ADVERTISING_HPP

#include <meta_tools.hpp>
#include "address.hpp"
#include "buffer.hpp"
#include "delta_time.hpp"
#include "ll_meta_types.hpp"

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
            static constexpr std::size_t    maximum_adv_request_size    = 34 + advertising_pdu_header_size;

            static bool is_valid_scan_request( const read_buffer& receive, const device_address& addr )
            {
                static constexpr std::size_t  scan_request_size = 2 * address_length + advertising_pdu_header_size;
                static constexpr std::uint8_t scan_request_code = 0x03;

                bool result = receive.size == scan_request_size
                    && ( receive.buffer[ 1 ] & 0x3f ) == scan_request_size - advertising_pdu_header_size
                    && ( receive.buffer[ 0 ] & 0x0f ) == scan_request_code;

                result = result && std::equal( &receive.buffer[ 8 ], &receive.buffer[ 14 ], addr.begin() );
                result = result && addr.is_random() == ( ( receive.buffer[ 0 ] & header_rxaddr_field ) != 0 );

                return result;
            }

            static bool is_valid_connect_request( const read_buffer& receive, const device_address& addr )
            {
                static constexpr std::size_t  connect_request_size = 34 + advertising_pdu_header_size;
                static constexpr std::uint8_t connect_request_code = 0x05;

                bool result = receive.size == connect_request_size
                        && ( receive.buffer[ 1 ] & 0x3f ) == connect_request_size - advertising_pdu_header_size
                        && ( receive.buffer[ 0 ] & 0x0f ) == connect_request_code;

                result = result && std::equal( &receive.buffer[ 8 ], &receive.buffer[ 14 ], addr.begin() );
                result = result && addr.is_random() == ( ( receive.buffer[ 0 ] & header_rxaddr_field ) != 0 );

                return result;
            }

            static std::size_t fill_empty_advertising_response_data( const device_address& addr, std::uint8_t* adv_response_buffer )
            {
                adv_response_buffer[ 0 ] = scan_response_pdu_type_code;

                if ( addr.is_random() )
                    adv_response_buffer[ 0 ] |= header_txaddr_field;

                std::size_t adv_response_size = advertising_pdu_header_size + address_length;

                std::copy( addr.begin(), addr.end(), &adv_response_buffer[ 2 ] );

                // add aditional empty AD to be visible to Nordic sniffer.
                // Some stacks do not recognize the response without this empty AD.
                adv_response_buffer[ adv_response_size ] = 0;
                adv_response_buffer[ adv_response_size + 1 ] = 0;
                adv_response_size += 2;

                adv_response_buffer[ 1 ] = adv_response_size - advertising_pdu_header_size;

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

        template < typename LinkLayer, typename >
        class impl : protected details::advertising_type_base
        {
        protected:
            read_buffer fill_advertising_data()
            {
                const device_address& addr = link_layer().local_address();

                std::uint8_t* const adv_data = advertising_buffer().buffer;
                adv_data[ 0 ] = adv_ind_pdu_type_code;

                if ( addr.is_random() )
                    adv_data[ 0 ] |= header_txaddr_field;

                adv_data[ 1 ] =
                    address_length
                  + link_layer().fill_l2cap_advertising_data( &adv_data[ advertising_pdu_header_size + address_length ], max_advertising_data_size );

                adv_size_ = advertising_pdu_header_size + adv_data[ 1 ];

                std::copy( addr.begin(), addr.end(), &adv_data[ 2 ] );

                fill_advertising_response_data();
                return advertising_buffer();
            }

            read_buffer get_advertising_data()
            {
                return advertising_buffer();
            }

            read_buffer get_advertising_response_data()
            {
                return advertising_response_buffer();
            }

            read_buffer advertising_receive_buffer()
            {
                return read_buffer{ advertising_response_buffer().buffer + maximum_adv_send_size, maximum_adv_request_size };
            }

            bool is_valid_scan_request( const read_buffer& receive ) const
            {
                return details::advertising_type_base::is_valid_scan_request( receive, link_layer().local_address() );
            }

            bool is_valid_connect_request( const read_buffer& receive ) const
            {
                return details::advertising_type_base::is_valid_connect_request( receive, link_layer().local_address() );
            }

            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size + address_length;
            static constexpr std::size_t    maximum_required_advertising_buffer = 2 * maximum_adv_send_size + maximum_adv_request_size;
        private:

            void fill_advertising_response_data()
            {
                adv_response_size_ = fill_empty_advertising_response_data(
                    link_layer().local_address(), advertising_response_buffer().buffer );
            }

            read_buffer advertising_buffer()
            {
                return read_buffer{ link_layer().raw(), adv_size_ };
            }

            read_buffer advertising_response_buffer()
            {
                return read_buffer{ advertising_buffer().buffer + maximum_adv_send_size, adv_response_size_ };
            }

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

            const LinkLayer& link_layer() const
            {
                return static_cast< const LinkLayer& >( *this );
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

    public:
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

        template < typename LinkLayer, typename Advertising >
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
                    static_cast< Advertising& >( *this ).handle_start_advertising();
            }

        protected:
            read_buffer advertising_receive_buffer()
            {
                return read_buffer{ advertising_buffer().buffer + maximum_adv_send_size, maximum_adv_request_size };
            }

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

                std::uint8_t* const adv_data = advertising_buffer().buffer;
                adv_data[ 0 ] = adv_direct_ind_pdu_type_code;

                if ( addr.is_random() )
                    adv_data[ 0 ] |= header_txaddr_field;

                if ( addr_.is_random() )
                    adv_data[ 0 ] |= header_rxaddr_field;

                adv_data[ 1 ] = 2 * address_length;

                std::copy( addr.begin(), addr.end(), &adv_data[ advertising_pdu_header_size ] );
                std::copy( addr_.begin(), addr_.end(), &adv_data[ advertising_pdu_header_size + address_length ] );

                return advertising_buffer();
            }

            read_buffer get_advertising_data()
            {
                return addr_valid_ ? advertising_buffer() : read_buffer{ nullptr, 0 };
            }

            read_buffer get_advertising_response_data() const
            {
                return read_buffer{ nullptr, 0 };
            }

            bool is_valid_scan_request( const read_buffer& ) const
            {
                return false;
            }

            bool is_valid_connect_request( const read_buffer& receive ) const
            {
                bool result = details::advertising_type_base::is_valid_connect_request( receive, link_layer().local_address() );
                result = result && std::equal( &receive.buffer[ 2 ], &receive.buffer[ 8 ], addr_.begin() ) && addr_valid_;
                result = result && addr_.is_random() == ( ( receive.buffer[ 0 ] & header_txaddr_field ) != 0 );

                return result;
            }

            static constexpr std::size_t    max_advertising_data_size   = 2 * address_length;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size;
            static constexpr std::size_t    maximum_required_advertising_buffer = maximum_adv_send_size + maximum_adv_request_size;
        private:

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

            const LinkLayer& link_layer() const
            {
                return static_cast< const LinkLayer& >( *this );
            }

            read_buffer advertising_buffer()
            {
                return read_buffer{ link_layer().raw(), maximum_adv_send_size };
            }

            read_buffer advertising_response_buffer()
            {
                return read_buffer{ nullptr, 0 };
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

        template < typename LinkLayer, typename >
        class impl : protected details::advertising_type_base
        {
        protected:
            read_buffer fill_advertising_data()
            {
                fill_advertising_response_data();
                const device_address& addr = link_layer().local_address();

                std::uint8_t* const adv_data = advertising_buffer().buffer;
                adv_data[ 0 ] = adv_scan_ind_pdu_type_code;

                if ( addr.is_random() )
                    adv_data[ 0 ] |= header_txaddr_field;

                adv_data[ 1 ] =
                    address_length
                  + link_layer().fill_l2cap_advertising_data( &adv_data[ advertising_pdu_header_size + address_length ], max_advertising_data_size );

                adv_size_ = advertising_pdu_header_size + adv_data[ 1 ];

                std::copy( addr.begin(), addr.end(), &adv_data[ 2 ] );

                return advertising_buffer();
            }

            read_buffer get_advertising_data()
            {
                return advertising_buffer();
            }

            read_buffer get_advertising_response_data()
            {
                return advertising_response_buffer();
            }

            read_buffer advertising_receive_buffer()
            {
                return read_buffer{ advertising_response_buffer().buffer + maximum_adv_send_size, maximum_adv_request_size };
            }

            bool is_valid_scan_request( const read_buffer& receive ) const
            {
                return details::advertising_type_base::is_valid_scan_request( receive, link_layer().local_address() );
            }

            bool is_valid_connect_request( const read_buffer& ) const
            {
                return false;
            }

            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size + address_length;
            static constexpr std::size_t    maximum_required_advertising_buffer = 2 * maximum_adv_send_size + maximum_adv_request_size;

        private:
            void fill_advertising_response_data()
            {
                adv_response_size_ = fill_empty_advertising_response_data(
                    link_layer().local_address(), advertising_response_buffer().buffer );
            }

            read_buffer advertising_buffer()
            {
                return read_buffer{ link_layer().raw(), adv_size_ };
            }

            read_buffer advertising_response_buffer()
            {
                return read_buffer{ advertising_buffer().buffer + maximum_adv_send_size, adv_response_size_ };
            }

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

            const LinkLayer& link_layer() const
            {
                return static_cast< const LinkLayer& >( *this );
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

        template < typename LinkLayer, typename >
        class impl : protected details::advertising_type_base
        {
        protected:
            read_buffer fill_advertising_data()
            {
                const device_address& addr = link_layer().local_address();

                std::uint8_t* const adv_data = advertising_buffer().buffer;
                adv_data[ 0 ] = adv_nonconn_ind_pdu_type_code;

                if ( addr.is_random() )
                    adv_data[ 0 ] |= header_txaddr_field;

                adv_data[ 1 ] =
                    address_length
                  + link_layer().fill_l2cap_advertising_data( &adv_data[ advertising_pdu_header_size + address_length ], max_advertising_data_size );

                adv_size_ = advertising_pdu_header_size + adv_data[ 1 ];

                std::copy( addr.begin(), addr.end(), &adv_data[ 2 ] );

                return advertising_buffer();
            }

            read_buffer get_advertising_data()
            {
                return advertising_buffer();
            }

            read_buffer get_advertising_response_data() const
            {
                return read_buffer{ nullptr, 0 };
            }

            read_buffer advertising_receive_buffer()
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

            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size + address_length;
            static constexpr std::size_t    maximum_required_advertising_buffer = maximum_adv_send_size;
        private:

            read_buffer advertising_buffer()
            {
                return read_buffer{ link_layer().raw(), adv_size_ };
            }

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
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
     * @brief adds the abillity to change the advertising channel
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
        /*
         * Type to implement the single and multiple adverting type advertisings
         */
        template < typename LinkLayer, typename Options, typename ... Advertisings >
        class advertiser;

        struct advertiser_base_base
        {
            static constexpr std::uint32_t  advertising_radio_access_address = 0x8E89BED6;
            static constexpr std::uint32_t  advertising_crc_init             = 0x555555;

            static constexpr unsigned       first_advertising_channel   = 37;
            static constexpr unsigned       last_advertising_channel    = 39;
        };

        template < typename ... Options >
        class advertiser_base :
            public advertiser_base_base,
            public bluetoe::details::find_by_meta_type<
                    details::advertising_interval_meta_type,
                    Options..., advertising_interval< 100 > >::type
        {
        protected:
            advertiser_base()
                : current_channel_index_( first_advertising_channel )
                , adv_perturbation_( 0 )
            {
            }

            delta_time next_adv_event()
            {
                if ( current_channel_index_ != this->first_advertising_channel )
                    return delta_time::now();

                adv_perturbation_ = ( adv_perturbation_ + 7 ) % ( max_adv_perturbation_ + 1 );

                return this->current_advertising_interval() + delta_time::msec( adv_perturbation_ );
            }

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

        private:
            static constexpr unsigned       max_adv_perturbation_ = 10;

            unsigned                        current_channel_index_;
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
            public advertiser_base< Options... >,
            public start_stop_implementation< LinkLayer, std::tuple< Advertising >, Options... >
        {
        public:

            /*
             * Send out, first advertising
             */
            void handle_start_advertising()
            {
                const read_buffer advertising_data = this->fill_advertising_data();
                const read_buffer response_data    = this->get_advertising_response_data();

                if ( !advertising_data.empty() && this->begin_of_advertising_events() )
                {
                    LinkLayer& link_layer  = static_cast< LinkLayer& >( *this );

                    link_layer.set_access_address_and_crc_init(
                        this->advertising_radio_access_address,
                        this->advertising_crc_init );

                    link_layer.schedule_advertisment(
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
                LinkLayer& link_layer  = static_cast< LinkLayer& >( *this );

                if ( this->is_valid_connect_request( receive ) )
                {
                    remote_address = device_address( &receive.buffer[ 2 ], receive.buffer[ 0 ] & 0x40 );

                    if ( link_layer.is_connection_request_in_filter( remote_address ) )
                        return true;
                }

                handle_adv_timeout();

                return false;
            }

            void handle_adv_timeout()
            {
                const read_buffer advertising_data = this->get_advertising_data();
                const read_buffer response_data    = this->get_advertising_response_data();

                if ( !advertising_data.empty() && this->continued_advertising_events() )
                {
                    this->next_channel();

                    static_cast< LinkLayer& >( *this ).schedule_advertisment(
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

            read_buffer advertising_receive_buffer( unsigned )
            {
                return read_buffer{ nullptr, 0 };
            }

            read_buffer get_advertising_data( unsigned )
            {
                return read_buffer{ nullptr, 0 };
            }

            read_buffer get_advertising_response_data( unsigned )
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

            read_buffer advertising_receive_buffer( unsigned selected )
            {
                return selected == 0
                    ? adv_type::advertising_receive_buffer()
                    : tail_type::advertising_receive_buffer( selected -1 );
            }

            read_buffer get_advertising_data( unsigned selected )
            {
                return selected == 0
                    ? adv_type::get_advertising_data()
                    : tail_type::get_advertising_data( selected -1 );
            }

            read_buffer get_advertising_response_data( unsigned selected )
            {
                return selected == 0
                    ? adv_type::get_advertising_response_data()
                    : tail_type::get_advertising_response_data( selected -1 );
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
            public advertiser_base< Options... >,
            public multipl_advertiser_base<
                LinkLayer,
                std::tuple< Options... >,
                advertiser< LinkLayer, std::tuple< Options... >, std::tuple< FirstAdv, SecondAdv, Advertisings... > >,
                FirstAdv, SecondAdv, Advertisings...
            >,
            public start_stop_implementation< LinkLayer, std::tuple< FirstAdv, SecondAdv, Advertisings... >, Options... >
        {
        public:
            static constexpr std::size_t maximum_required_advertising_buffer = 100;

            advertiser()
                : selected_( 0 )
                , proposal_( 0 )
            {
            }

            void handle_start_advertising()
            {
                selected_ = proposal_;
                const read_buffer advertising_data = this->fill_advertising_data( selected_ );
                const read_buffer response_data    = this->get_advertising_response_data( selected_ );

                if ( !advertising_data.empty() && this->begin_of_advertising_events() )
                {
                    LinkLayer& link_layer  = static_cast< LinkLayer& >( *this );

                    link_layer.set_access_address_and_crc_init(
                        this->advertising_radio_access_address,
                        this->advertising_crc_init );

                    link_layer.schedule_advertisment(
                        this->current_channel(),
                        write_buffer( advertising_data ),
                        write_buffer( response_data ),
                        delta_time::now(),
                        this->advertising_receive_buffer( selected_ ) );
                }
            }

            void handle_stop_advertising()
            {
                this->end_of_advertising_events();
            }

            bool handle_adv_receive( read_buffer receive, device_address& remote_address )
            {
                LinkLayer& link_layer  = static_cast< LinkLayer& >( *this );

                if ( this->is_valid_connect_request( receive, selected_ ) )
                {
                    remote_address = device_address( &receive.buffer[ 2 ], receive.buffer[ 0 ] & 0x40 );

                    if ( link_layer.is_connection_request_in_filter( remote_address ) )
                        return true;
                }

                handle_adv_timeout();

                return false;
            }

            void handle_adv_timeout()
            {
                const read_buffer advertising_data = this->get_advertising_data( selected_ );
                const read_buffer response_data    = this->get_advertising_response_data( selected_ );

                if ( !advertising_data.empty() && this->continued_advertising_events() )
                {
                    this->next_channel();

                    static_cast< LinkLayer& >( *this ).schedule_advertisment(
                        this->current_channel(),
                        write_buffer( advertising_data ),
                        write_buffer( response_data ),
                        this->next_adv_event(),
                        this->advertising_receive_buffer( selected_ ) );
                }
            }

            /*
             * this is the true implementation of all the documentated function within the advertising types
             */
            template < typename Type >
            void change_advertising()
            {
                proposal_ = bluetoe::details::index_of< Type, FirstAdv, SecondAdv, Advertisings... >::value;
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
