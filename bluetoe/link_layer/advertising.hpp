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
                result = result && addr.is_random() == ( ( receive.buffer[ 0 ] & header_txaddr_field ) != 0 );

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
                result = result && addr.is_random() == ( ( receive.buffer[ 0 ] & header_txaddr_field ) != 0 );

                return result;
            }
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
    class connectable_undirected_advertising
    {
        /** @cond HIDDEN_SYMBOLS */
    public:
        typedef details::advertising_type_meta_type meta_type;

        template < typename LinkLayer, typename >
        class impl : protected details::advertising_type_base
        {
        protected:
            bool fill_advertising_data()
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

                return true;
            }

            bool get_advertising_data() const
            {
                return true;
            }

            bool fill_advertising_response_data()
            {
                const device_address& addr = link_layer().local_address();

                std::uint8_t* adv_response_buffer = advertising_response_buffer().buffer;

                adv_response_buffer[ 0 ] = scan_response_pdu_type_code;

                if ( addr.is_random() )
                    adv_response_buffer[ 0 ] |= header_txaddr_field;

                adv_response_buffer[ 1 ] = address_length;
                adv_response_size_ = advertising_pdu_header_size + adv_response_buffer[ 1 ];

                std::copy( addr.begin(), addr.end(), &adv_response_buffer[ 2 ] );

                return true;
            }

            bool get_advertising_response_data() const
            {
                return true;
            }

            read_buffer advertising_buffer()
            {
                return read_buffer{ link_layer().raw(), adv_size_ };
            }

            read_buffer advertising_response_buffer()
            {
                return read_buffer{ advertising_buffer().buffer + maximum_adv_send_size, adv_response_size_ };
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
        private:
            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size + address_length;
            static constexpr std::size_t    maximum_required_advertising_buffer = 2 * maximum_adv_send_size + maximum_adv_request_size;

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
         * @brief sets the address to be used in the advertising
         *        PDU.
         *
         * Starts advertising if an address was not set before.
         */
        void directed_advertising_address( const device_address& addr );

        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_type_meta_type meta_type;

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
            read_buffer advertising_buffer()
            {
                return read_buffer{ link_layer().raw(), maximum_adv_send_size };
            }

            read_buffer advertising_response_buffer()
            {
                return read_buffer{ nullptr, 0 };
            }

            read_buffer advertising_receive_buffer()
            {
                return read_buffer{ advertising_buffer().buffer + maximum_adv_send_size, maximum_adv_request_size };
            }

            impl()
                : addr_valid_( false )
                , started_( false )
            {
            }

            bool fill_advertising_data()
            {
                if ( !addr_valid_ )
                {
                    started_ = true;
                    return false;
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

                return true;
            }

            bool get_advertising_data() const
            {
                return addr_valid_;
            }

            bool fill_advertising_response_data()
            {
                return false;
            }

            bool get_advertising_response_data() const
            {
                return false;
            }

            bool is_valid_scan_request( const read_buffer& receive ) const
            {
                return false;
            }

            bool is_valid_connect_request( const read_buffer& receive ) const
            {
                bool result = details::advertising_type_base::is_valid_connect_request( receive, link_layer().local_address() );
                result = result && std::equal( &receive.buffer[ 2 ], &receive.buffer[ 8 ], addr_.begin() ) && addr_valid_;
                result = result && addr_.is_random() == ( ( receive.buffer[ 0 ] & header_rxaddr_field ) != 0 );

                return result;
            }
        private:
            static constexpr std::size_t    max_advertising_data_size   = 2 * address_length;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size;

            LinkLayer& link_layer()
            {
                return static_cast< LinkLayer& >( *this );
            }

            const LinkLayer& link_layer() const
            {
                return static_cast< const LinkLayer& >( *this );
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
        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_type_meta_type meta_type;

        template < typename LinkLayer, typename >
        class impl : protected details::advertising_type_base
        {
        protected:
            bool fill_advertising_data()
            {
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

                return true;
            }

            bool get_advertising_data() const
            {
                return true;
            }

            bool fill_advertising_response_data()
            {
                const device_address& addr = link_layer().local_address();

                std::uint8_t* adv_response_buffer = advertising_response_buffer().buffer;

                adv_response_buffer[ 0 ] = scan_response_pdu_type_code;

                if ( addr.is_random() )
                    adv_response_buffer[ 0 ] |= header_txaddr_field;

                adv_response_buffer[ 1 ] = address_length;
                adv_response_size_ = advertising_pdu_header_size + adv_response_buffer[ 1 ];

                std::copy( addr.begin(), addr.end(), &adv_response_buffer[ 2 ] );

                return true;
            }

            bool get_advertising_response_data() const
            {
                return true;
            }

            read_buffer advertising_buffer()
            {
                return read_buffer{ link_layer().raw(), adv_size_ };
            }

            read_buffer advertising_response_buffer()
            {
                return read_buffer{ advertising_buffer().buffer + maximum_adv_send_size, adv_response_size_ };
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
                return false;
            }
        private:
            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size + address_length;
            static constexpr std::size_t    maximum_required_advertising_buffer = 2 * maximum_adv_send_size + maximum_adv_request_size;

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
        /** @cond HIDDEN_SYMBOLS */
        typedef details::advertising_type_meta_type meta_type;

        template < typename LinkLayer, typename >
        class impl : protected details::advertising_type_base
        {
        protected:
            bool fill_advertising_data()
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

                return true;
            }

            bool get_advertising_data() const
            {
                return true;
            }

            bool fill_advertising_response_data()
            {
                return false;
            }

            bool get_advertising_response_data() const
            {
                return false;
            }

            read_buffer advertising_buffer()
            {
                return read_buffer{ link_layer().raw(), adv_size_ };
            }

            read_buffer advertising_response_buffer()
            {
                return read_buffer{ nullptr, 0 };
            }

            read_buffer advertising_receive_buffer()
            {
                return read_buffer{ nullptr, 0 };
            }

            bool is_valid_scan_request( const read_buffer& receive ) const
            {
                return false;
            }

            bool is_valid_connect_request( const read_buffer& receive ) const
            {
                return false;
            }
        private:
            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size + address_length;
            static constexpr std::size_t    maximum_required_advertising_buffer = maximum_adv_send_size;

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
        template < typename LinkLayer, typename ... Options >
        class advertising_state_impl :
            public bluetoe::details::find_by_meta_type< advertising_type_meta_type,
                Options..., connectable_undirected_advertising >::type::template impl< LinkLayer, advertising_state_impl< LinkLayer, Options... > >
        {
        public:
            static constexpr std::size_t    max_advertising_data_size   = 31;
            static constexpr std::size_t    advertising_pdu_header_size = 2;
            static constexpr std::size_t    address_length              = 6;
            static constexpr std::size_t    maximum_adv_send_size       = max_advertising_data_size + advertising_pdu_header_size + address_length;
            static constexpr std::size_t    maximum_adv_request_size    = 34 + advertising_pdu_header_size;
            static constexpr std::size_t    maximum_required_advertising_buffer = 2 * maximum_adv_send_size + maximum_adv_request_size;

            advertising_state_impl()
                : current_channel_index_( first_advertising_channel )
                , adv_perturbation_( 0 )
            {
            }

            /*
             * Send out, first advertising
             */
            void handle_start_advertising()
            {
                current_channel_index_ = first_advertising_channel;
                LinkLayer& link_layer  = static_cast< LinkLayer& >( *this );

                if ( this->fill_advertising_data() )
                {
                    link_layer.set_access_address_and_crc_init(
                        advertising_radio_access_address,
                        advertising_crc_init );

                    link_layer.schedule_advertisment_and_receive(
                        current_channel_index_,
                        write_buffer( this->advertising_buffer() ),
                        delta_time::now(),
                        this->advertising_receive_buffer() );
                }
            }

            /*
             * handling incomming PDU
             */
            bool handle_adv_receive( read_buffer receive, device_address& remote_address )
            {
                LinkLayer& link_layer  = static_cast< LinkLayer& >( *this );

                if ( this->is_valid_scan_request( receive ) )
                {
                    remote_address = device_address( &receive.buffer[ 2 ], receive.buffer[ 0 ] & 0x40 );

                    if ( link_layer.is_scan_request_in_filter( remote_address )
                     &&  this->fill_advertising_response_data() )
                    {
                        link_layer.schedule_advertisment_and_receive(
                            current_channel_index_,
                            write_buffer( this->advertising_response_buffer() ),
                            delta_time::now(),
                            read_buffer{ nullptr, 0 } );

                        return false;
                    }
                }
                else if ( this->is_valid_connect_request( receive ) )
                {
                    remote_address = device_address( &receive.buffer[ 2 ], receive.buffer[ 0 ] & 0x40 );
                    return true;
                }

                handle_adv_timeout();

                return false;
            }

            void handle_adv_timeout()
            {
                LinkLayer& link_layer  = static_cast< LinkLayer& >( *this );

                current_channel_index_ = current_channel_index_ == last_advertising_channel
                    ? first_advertising_channel
                    : current_channel_index_ + 1;

                const delta_time next_time = current_channel_index_ == first_advertising_channel
                    ? next_adv_event()
                    : delta_time::now();

                if ( this->get_advertising_data() )
                {
                    link_layer.schedule_advertisment_and_receive(
                        current_channel_index_,
                        write_buffer( this->advertising_buffer() ),
                        next_time,
                        this->advertising_receive_buffer() );
                }
            }

        private:
            typedef                         advertising_interval< 100 >         default_advertising_interval;

            static constexpr std::uint32_t  advertising_radio_access_address = 0x8E89BED6;
            static constexpr std::uint32_t  advertising_crc_init             = 0x555555;

            static constexpr unsigned       first_advertising_channel   = 37;
            static constexpr unsigned       last_advertising_channel    = 39;
            static constexpr unsigned       max_adv_perturbation_       = 10;

            unsigned                        current_channel_index_;
            unsigned                        adv_perturbation_;

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
