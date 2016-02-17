#ifndef BLUETOE_LINK_LAYER_LINK_LAYER_HPP
#define BLUETOE_LINK_LAYER_LINK_LAYER_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/link_layer/address.hpp>
#include <bluetoe/link_layer/channel_map.hpp>
#include <bluetoe/link_layer/notification_queue.hpp>
#include <bluetoe/attribute.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/sm/security_manager.hpp>

#include <algorithm>
#include <cassert>

namespace bluetoe {
    namespace details {
        class notification_data;
    }

namespace link_layer {

    namespace details {
        template < typename ... Options >
        struct buffer_sizes
        {
            typedef typename ::bluetoe::details::find_by_meta_type<
                buffer_sizes_meta_type,
                Options...,
                ::bluetoe::link_layer::buffer_sizes<>  // default
            >::type s_type;

            static constexpr std::size_t tx_size = s_type::transmit_buffer_size;
            static constexpr std::size_t rx_size = s_type::receive_buffer_size;
        };

        template < typename ... Options >
        struct security_manager {
            typedef typename bluetoe::details::find_by_meta_type<
                bluetoe::details::security_manager_meta_type,
                Options...,
                no_security_manager >::type type;
        };

    }

    /**
     * @brief link layer implementation
     *
     * Implements a binding to a server by implementing a link layout on top of a ScheduleRadio device.
     */
    template <
        class Server,
        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBack
        >
        class ScheduledRadio,
        typename ... Options
    >
    class link_layer : public ScheduledRadio< details::buffer_sizes< Options... >::tx_size, details::buffer_sizes< Options... >::rx_size, link_layer< Server, ScheduledRadio, Options... > >,
        public details::security_manager< Options... >::type
    {
    public:
        link_layer();

        /**
         * @brief this function passes the CPU to the link layer implementation
         *
         * This function should return on certain events to alow user code to do
         * usefull things. Details depend on the ScheduleRadio implemention.
         */
        void run( Server& );

        /**
         * @brief call back that will be called when the master responds to an advertising PDU
         * @sa scheduled_radio::schedule_advertisment_and_receive
         */
        void adv_received( const read_buffer& receive );

        /**
         * @brief call back that will be called when the master does not respond to an advertising PDU
         * @sa scheduled_radio::schedule_advertisment_and_receive
         */
        void adv_timeout();

        /**
         * @brief call back that will be called when connect event times out
         * @sa scheduled_radio::schedule_connection_event
         */
        void timeout();

        /**
         * @brief call back that will be called after a connect event was closed.
         * @sa scheduled_radio::schedule_connection_event
         */
        void end_event();
    private:
        typedef ScheduledRadio<
            details::buffer_sizes< Options... >::tx_size,
            details::buffer_sizes< Options... >::rx_size,
            link_layer< Server, ScheduledRadio, Options... > > radio_t;

        typedef notification_queue<
            Server::number_of_client_configs,
            typename Server::connection_data > notification_queue_t;

        typedef typename details::security_manager< Options... >::type security_manager_t;

        // calculates the time point for the next advertising event
        delta_time next_adv_event();

        void fill_advertising_buffer();
        void fill_advertising_response_buffer();
        bool is_valid_scan_request( const read_buffer& receive ) const;
        bool is_valid_connect_request( const read_buffer& receive ) const;
        unsigned sleep_clock_accuracy( const read_buffer& receive ) const;
        bool check_timing_paremeters( std::uint16_t slave_latency, delta_time timeout ) const;
        bool parse_timing_parameters_from_connect_request( const read_buffer& valid_connect_request );
        bool parse_timing_parameters_from_connection_update_request( const write_buffer& valid_connect_request );
        void start_advertising();
        void wait_for_connection_event();
        void transmit_notifications();
        static void lcap_notification_callback( const ::bluetoe::details::notification_data& item, void* usr_arg, typename Server::notification_type type );

        std::uint8_t* advertising_buffer();
        std::uint8_t* advertising_response_buffer();
        std::uint8_t* advertising_receive_buffer();

        enum class ll_result {
            go_ahead,
            disconnect
        };

        ll_result handle_received_data();
        ll_result handle_ll_control_data( const write_buffer& pdu );
        ll_result handle_l2cap( const write_buffer& pdu );
        ll_result handle_pending_ll_control();

        static std::uint16_t read_16( const std::uint8_t* );
        static std::uint32_t read_24( const std::uint8_t* );
        static std::uint32_t read_32( const std::uint8_t* );

        static constexpr std::size_t    max_advertising_data_size   = 31;
        static constexpr std::size_t    advertising_pdu_header_size = 2;
        static constexpr std::size_t    address_length              = 6;
        static constexpr std::size_t    maximum_adv_request_size    = 34 + advertising_pdu_header_size;

        static constexpr unsigned       first_advertising_channel   = 37;
        static constexpr unsigned       last_advertising_channel    = 39;
        static constexpr unsigned       max_adv_perturbation_       = 10;
        static constexpr unsigned       num_windows_til_timeout     = 5;

        static constexpr std::uint8_t   adv_ind_pdu_type_code       = 0;
        static constexpr std::uint8_t   scan_response_pdu_type_code = 4;
        static constexpr std::uint8_t   ll_control_pdu_code         = 3;
        static constexpr std::uint8_t   lld_data_pdu_code           = 2;

        static constexpr std::uint8_t   header_txaddr_field         = 0x40;

        static constexpr std::uint32_t  advertising_radio_access_address = 0x8E89BED6;
        static constexpr std::uint32_t  advertising_crc_init             = 0x555555;

        static constexpr std::uint8_t   LL_CONNECTION_UPDATE_REQ    = 0x00;
        static constexpr std::uint8_t   LL_CHANNEL_MAP_REQ          = 0x01;
        static constexpr std::uint8_t   LL_TERMINATE_IND            = 0x02;
        static constexpr std::uint8_t   LL_UNKNOWN_RSP              = 0x07;
        static constexpr std::uint8_t   LL_FEATURE_REQ              = 0x08;
        static constexpr std::uint8_t   LL_FEATURE_RSP              = 0x09;
        static constexpr std::uint8_t   LL_VERSION_IND              = 0x0C;
        static constexpr std::uint8_t   LL_PING_REQ                 = 0x12;
        static constexpr std::uint8_t   LL_PING_RSP                 = 0x13;

        static constexpr std::uint8_t   LL_VERSION_NR               = 0x08;

        static constexpr std::uint16_t  l2cap_gap_channel           = 4;
        static constexpr std::uint16_t  l2cap_signaling_channel     = 5;
        static constexpr std::uint16_t  l2cap_sm_channel            = 6;

        static constexpr std::size_t    l2cap_header_size           = 4;
        static constexpr std::size_t    all_header_size             = 6;


        // TODO: calculate the actual needed buffer size for advertising, not the maximum
        static_assert( radio_t::size >= 2 * ( max_advertising_data_size + address_length + advertising_pdu_header_size ) + maximum_adv_request_size, "buffer to small" );

        std::size_t                     adv_size_;
        std::size_t                     adv_response_size_;

        unsigned                        current_channel_index_;
        unsigned                        adv_perturbation_;
        const address                   address_;
        channel_map                     channels_;
        unsigned                        cumulated_sleep_clock_accuracy_;
        delta_time                      transmit_window_offset_;
        delta_time                      transmit_window_size_;
        delta_time                      connection_interval_;
        delta_time                      connection_interval_old_;
        std::uint16_t                   conn_event_counter_;
        std::uint16_t                   defered_conn_event_counter_;
        write_buffer                    defered_ll_control_pdu_;
        unsigned                        timeouts_til_connection_lost_;
        unsigned                        max_timeouts_til_connection_lost_;
        Server*                         server_;
        notification_queue_t            connection_details_;

        enum class state
        {
            initial,
            advertising,
            connecting,
            connection_update,
            connected
        }                               state_;

        // default configuration parameters
        typedef                         advertising_interval< 100 >         default_advertising_interval;
        typedef                         sleep_clock_accuracy_ppm< 500 >     default_sleep_clock_accuracy;
        typedef                         random_static_address               default_device_address;

        typedef typename ::bluetoe::details::find_by_meta_type<
            details::device_address_meta_type,
            Options..., default_device_address >::type              device_address;

        typedef typename ::bluetoe::details::find_by_meta_type<
            details::sleep_clock_accuracy_meta_type,
            Options..., default_sleep_clock_accuracy >::type        device_sleep_clock_accuracy;

    };

    // implementation
    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    link_layer< Server, ScheduledRadio, Options... >::link_layer()
        : adv_size_( 0 )
        , adv_response_size_( 0 )
        , current_channel_index_( first_advertising_channel )
        , adv_perturbation_( 0 )
        , address_( device_address::address( *this ) )
        , defered_ll_control_pdu_{ nullptr, 0 }
        , server_( nullptr )
        , connection_details_( 23 )
        , state_( state::initial )
    {
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::run( Server& server )
    {
        // after the initial scheduling, the timeout and receive callback will setup the next scheduling
        if ( state_ == state::initial )
        {
            server_ = &server;
            start_advertising();

            server.notification_callback( lcap_notification_callback, this );
        }

        radio_t::run();

        if ( state_ == state::connected )
            transmit_notifications();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::adv_received( const read_buffer& receive )
    {
        assert( state_ == state::advertising );

        if ( is_valid_scan_request( receive ) )
        {
            this->schedule_advertisment_and_receive(
                current_channel_index_,
                write_buffer{ advertising_response_buffer(), adv_response_size_ }, delta_time::now(),
                read_buffer{ nullptr, 0 } );
        }
        else if (
               is_valid_connect_request( receive )
            && channels_.reset( &receive.buffer[ 30 ], receive.buffer[ 35 ] & 0x1f )
            && parse_timing_parameters_from_connect_request( receive ) )
        {
            state_                    = state::connecting;
            current_channel_index_    = 0;
            conn_event_counter_       = 0;
            cumulated_sleep_clock_accuracy_ = sleep_clock_accuracy( receive ) + device_sleep_clock_accuracy::accuracy_ppm;
            timeouts_til_connection_lost_   = num_windows_til_timeout - 1;

            this->set_access_address_and_crc_init(
                read_32( &receive.buffer[ 14 ] ),
                read_24( &receive.buffer[ 18 ] ) );

            const delta_time window_start = transmit_window_offset_ - transmit_window_offset_.ppm( cumulated_sleep_clock_accuracy_ );
                  delta_time window_end   = transmit_window_offset_ + transmit_window_size_;

            window_end += window_end.ppm( cumulated_sleep_clock_accuracy_ );

            this->reset();
            this->schedule_connection_event(
                channels_.data_channel( current_channel_index_ ),
                window_start,
                window_end,
                connection_interval_ );
        }
        else
        {
            adv_timeout();
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::adv_timeout()
    {
        assert( state_ == state::advertising );

        current_channel_index_ = current_channel_index_ == last_advertising_channel
            ? first_advertising_channel
            : current_channel_index_ + 1;

        const delta_time next_time = current_channel_index_ == first_advertising_channel
            ? next_adv_event()
            : delta_time::now();

        this->schedule_advertisment_and_receive(
            current_channel_index_,
            write_buffer{ advertising_buffer(), adv_size_ }, next_time,
            read_buffer{ advertising_receive_buffer(), maximum_adv_request_size } );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::timeout()
    {
        assert( state_ == state::connecting || state_ == state::connected || state_ == state::connection_update);

        if ( timeouts_til_connection_lost_ )
        {
            current_channel_index_ = ( current_channel_index_ + 1 ) % first_advertising_channel;

            --timeouts_til_connection_lost_;
            ++conn_event_counter_;

            if ( handle_pending_ll_control() == ll_result::disconnect )
            {
                start_advertising();
            }
            else
            {
                wait_for_connection_event();
            }
        }
        else
        {
            start_advertising();
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::end_event()
    {
        assert( state_ == state::connecting || state_ == state::connected || state_ == state::connection_update );

        state_                        = state::connected;
        current_channel_index_        = ( current_channel_index_ + 1 ) % first_advertising_channel;
        timeouts_til_connection_lost_ = max_timeouts_til_connection_lost_;
        ++conn_event_counter_;

        if( handle_received_data() == ll_result::disconnect )
        {
            start_advertising();
        }
        else
        {
            wait_for_connection_event();
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::wait_for_connection_event()
    {
        delta_time window_start;
        delta_time window_end;

        if ( state_ == state::connecting )
        {
            const delta_time window_target = connection_interval_ * ( num_windows_til_timeout - timeouts_til_connection_lost_ - 1 );

            window_start = transmit_window_offset_ + window_target;
            window_end   = window_start + transmit_window_size_;

            window_start -= window_start.ppm( cumulated_sleep_clock_accuracy_ );
            window_end   += window_end.ppm( cumulated_sleep_clock_accuracy_ );
        }
        else if ( state_ == state::connection_update )
        {
            window_start = connection_interval_old_ + transmit_window_offset_;
            window_end   = window_start + transmit_window_size_;

            window_start -= window_start.ppm( cumulated_sleep_clock_accuracy_ );
            window_end   += window_end.ppm( cumulated_sleep_clock_accuracy_ );
        }
        else
        {
            const delta_time window_target = connection_interval_ * ( max_timeouts_til_connection_lost_ - timeouts_til_connection_lost_ + 1 );
            const delta_time window_size   = window_target.ppm( cumulated_sleep_clock_accuracy_ );

            window_start  = window_target - window_size;
            window_end    = window_target + window_size;
        }

        this->schedule_connection_event(
            channels_.data_channel( current_channel_index_ ),
            window_start,
            window_end,
            connection_interval_ );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::transmit_notifications()
    {
        // first check if we have memory to transmit the message, or otherwise notifications would get lost
        auto out_buffer = this->allocate_transmit_buffer();

        if ( out_buffer.empty() )
            return;

        const auto notification = connection_details_.dequeue_indication_or_confirmation();

        if ( notification.first != notification_queue_t::empty )
        {
            std::size_t out_size = out_buffer.size - all_header_size;

            server_->notification_output(
                &out_buffer.buffer[ all_header_size ],
                out_size,
                connection_details_,
                notification.second
            );

            if ( out_size )
            {
                out_buffer.buffer[ 0 ] = lld_data_pdu_code;
                out_buffer.buffer[ 1 ] = static_cast< std::uint8_t >( out_size + l2cap_header_size );
                out_buffer.buffer[ 2 ] = static_cast< std::uint8_t >( out_size );
                out_buffer.buffer[ 3 ] = 0;
                out_buffer.buffer[ 4 ] = static_cast< std::uint8_t >( l2cap_gap_channel );
                out_buffer.buffer[ 5 ] = static_cast< std::uint8_t >( l2cap_gap_channel >> 8 );

                this->commit_transmit_buffer( out_buffer );
            }
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::lcap_notification_callback( const ::bluetoe::details::notification_data& item, void* usr_arg, typename Server::notification_type type )
    {
        auto& confirmation_queue = static_cast< link_layer< Server, ScheduledRadio, Options... >* >( usr_arg )->connection_details_;
        switch ( type )
        {
            case Server::notification:
                confirmation_queue.queue_notification( item.client_characteristic_configuration_index() );
                break;
            case Server::indication:
                confirmation_queue.queue_indication( item.client_characteristic_configuration_index() );
                break;
            case Server::confirmation:
                confirmation_queue.indication_confirmed();
                break;
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    delta_time link_layer< Server, ScheduledRadio, Options... >::next_adv_event()
    {
        adv_perturbation_ = ( adv_perturbation_ + 7 ) % ( max_adv_perturbation_ + 1 );

        typedef typename ::bluetoe::details::find_by_meta_type<
            details::advertising_interval_meta_type,
            Options..., default_advertising_interval >::type adv_interval;

        return adv_interval::interval() + delta_time::msec( adv_perturbation_ );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::fill_advertising_buffer()
    {
        std::uint8_t* const adv_buffer = advertising_buffer();

        adv_buffer[ 0 ] = adv_ind_pdu_type_code;

        if ( device_address::is_random() )
            adv_buffer[ 0 ] |= header_txaddr_field;

        adv_buffer[ 1 ] = address_length + server_->advertising_data( &adv_buffer[ advertising_pdu_header_size + address_length ], max_advertising_data_size );
        std::copy( address_.begin(), address_.end(), &adv_buffer[ 2 ] );
        adv_size_ = advertising_pdu_header_size + adv_buffer[ 1 ];
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::fill_advertising_response_buffer()
    {
        std::uint8_t* adv_response_buffer = advertising_response_buffer();

        adv_response_buffer[ 0 ] = scan_response_pdu_type_code;

        if ( device_address::is_random() )
            adv_response_buffer[ 0 ] |= header_txaddr_field;

        adv_response_buffer[ 1 ] = address_length;

        std::copy( address_.begin(), address_.end(), &adv_response_buffer[ 2 ] );

        adv_response_size_ = advertising_pdu_header_size + adv_response_buffer[ 1 ];
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::is_valid_scan_request( const read_buffer& receive ) const
    {
        static constexpr std::size_t  scan_request_size = 2 * address_length + advertising_pdu_header_size;
        static constexpr std::uint8_t scan_request_code = 0x03;

        bool result = receive.size == scan_request_size
            && ( receive.buffer[ 1 ] & 0x3f ) == scan_request_size - advertising_pdu_header_size
            && ( receive.buffer[ 0 ] & 0x0f ) == scan_request_code;

        result = result && std::equal( &receive.buffer[ 8 ], &receive.buffer[ 14 ], address_.begin() );

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::is_valid_connect_request( const read_buffer& receive ) const
    {
        static constexpr std::size_t  connect_request_size = 34 + advertising_pdu_header_size;
        static constexpr std::uint8_t connect_request_code = 0x05;

        bool result = receive.size == connect_request_size
                && ( receive.buffer[ 1 ] & 0x3f ) == connect_request_size - advertising_pdu_header_size
                && ( receive.buffer[ 0 ] & 0x0f ) == connect_request_code;

        result = result && std::equal( &receive.buffer[ 8 ], &receive.buffer[ 14 ], address_.begin() );

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    unsigned link_layer< Server, ScheduledRadio, Options... >::sleep_clock_accuracy( const read_buffer& receive ) const
    {
        static constexpr std::uint16_t inaccuracy_ppm[ 8 ] = {
            500, 250, 150, 100, 75, 50, 30, 20
        };

        return inaccuracy_ppm[ ( receive.buffer[ 35 ] >> 5 & 0x7 )  ];
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::check_timing_paremeters( std::uint16_t slave_latency, delta_time timeout ) const
    {
        static constexpr delta_time maximum_transmit_window_offset( 10 * 1000 );
        static constexpr delta_time maximum_connection_timeout( 32 * 1000 * 1000 );
        static constexpr delta_time minimum_connection_timeout( 100 * 1000 );
        static constexpr auto       max_slave_latency = 499;

        return transmit_window_size_ <= maximum_transmit_window_offset
            && transmit_window_size_ <= connection_interval_
            && transmit_window_offset_ <= connection_interval_
            && timeout >= minimum_connection_timeout
            && timeout <= maximum_connection_timeout
            && timeout >= ( slave_latency + 1 ) * 2 * connection_interval_
            && slave_latency <= max_slave_latency;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::parse_timing_parameters_from_connect_request( const read_buffer& valid_connect_request )
    {
        static constexpr auto       us_per_digits = 1250;

        transmit_window_size_   = delta_time( valid_connect_request.buffer[ 21 ] * us_per_digits );
        transmit_window_offset_ = delta_time( read_16( &valid_connect_request.buffer[ 22 ] ) * us_per_digits + us_per_digits );
        connection_interval_    = delta_time( read_16( &valid_connect_request.buffer[ 24 ] ) * us_per_digits );
        auto slave_latency      = read_16( &valid_connect_request.buffer[ 26 ] );
        delta_time timeout      = delta_time( read_16( &valid_connect_request.buffer[ 28 ] ) * 10000 );

        max_timeouts_til_connection_lost_ = timeout / connection_interval_;

        return check_timing_paremeters( slave_latency, timeout );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    bool link_layer< Server, ScheduledRadio, Options... >::parse_timing_parameters_from_connection_update_request( const write_buffer& valid_update_request )
    {
        static constexpr auto       us_per_digits = 1250;

        transmit_window_size_   = delta_time( valid_update_request.buffer[ 3 ] * us_per_digits );
        transmit_window_offset_ = delta_time( read_16( &valid_update_request.buffer[ 4 ] ) * us_per_digits );
        connection_interval_    = delta_time( read_16( &valid_update_request.buffer[ 6 ] ) * us_per_digits );
        auto slave_latency      = read_16( &valid_update_request.buffer[ 8 ] );
        delta_time timeout      = delta_time( read_16( &valid_update_request.buffer[ 10 ] ) * 10000 );

        max_timeouts_til_connection_lost_ = timeout / connection_interval_;

        return check_timing_paremeters( slave_latency, timeout );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::start_advertising()
    {
        state_ = state::advertising;
        current_channel_index_ = first_advertising_channel;
        defered_ll_control_pdu_ = write_buffer{ nullptr, 0 };

        fill_advertising_buffer();
        fill_advertising_response_buffer();

        this->set_access_address_and_crc_init(
            advertising_radio_access_address,
            advertising_crc_init );

        this->schedule_advertisment_and_receive(
            current_channel_index_,
            write_buffer{ advertising_buffer(), adv_size_ }, delta_time::now(),
            read_buffer{ advertising_receive_buffer(), maximum_adv_request_size } );
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint8_t* link_layer< Server, ScheduledRadio, Options... >::advertising_buffer()
    {
        return this->raw();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint8_t* link_layer< Server, ScheduledRadio, Options... >::advertising_response_buffer()
    {
        return advertising_buffer() + max_advertising_data_size + address_length + advertising_pdu_header_size;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint8_t* link_layer< Server, ScheduledRadio, Options... >::advertising_receive_buffer()
    {
        return advertising_response_buffer() + max_advertising_data_size + address_length + advertising_pdu_header_size;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_received_data()
    {
        ll_result result = handle_pending_ll_control();

        if ( result != ll_result::go_ahead )
            return result;

        if ( defered_ll_control_pdu_.empty() )
        {
            for ( auto pdu = this->next_received(); pdu.size != 0; this->free_received(), pdu = this->next_received() )
            {
                const auto llid = pdu.buffer[ 0 ] & 0x03;
                if ( llid == ll_control_pdu_code )
                {
                    result = handle_ll_control_data( pdu );
                }
                else if ( llid == lld_data_pdu_code )
                {
                    result = handle_l2cap( pdu );
                }
            }
        }

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_ll_control_data( const write_buffer& pdu )
    {
        ll_result result = ll_result::go_ahead;
        bool      commit = true;

        // the allocated size could be optimized to the required size for the answer
        auto write = this->allocate_transmit_buffer();
        if ( write.size == 0 )
            return result;

        assert( write.size >= radio_t::min_buffer_size );

        if ( ( pdu.buffer[ 0 ] & 0x3 ) == ll_control_pdu_code )
        {

            const std::uint8_t size   = pdu.buffer[ 1 ];
            const std::uint8_t opcode = size > 0 ? pdu.buffer[ 2 ] : 0xff;

            if ( opcode == LL_CONNECTION_UPDATE_REQ && size == 12 )
            {
                defered_conn_event_counter_ = read_16( &pdu.buffer[ 12 ] );
                commit = false;

                if ( static_cast< std::uint16_t >( defered_conn_event_counter_ - conn_event_counter_ ) & 0x8000
                    || defered_conn_event_counter_ == conn_event_counter_ )
                {
                    result = ll_result::disconnect;
                }
                else
                {
                    defered_ll_control_pdu_ = pdu;
                }
            }
            else if ( opcode == LL_TERMINATE_IND && size == 2 )
            {
                commit = false;
                result = ll_result::disconnect;
            }
            else if ( opcode == LL_VERSION_IND && size == 6 )
            {
                write.fill( {
                    ll_control_pdu_code, 6, LL_VERSION_IND,
                    LL_VERSION_NR, 0x69, 0x02, 0x00, 0x00
                } );
            }
            else if ( opcode == LL_CHANNEL_MAP_REQ && size == 8 )
            {
                defered_conn_event_counter_ = read_16( &pdu.buffer[ 8 ] );
                commit = false;

                if ( static_cast< std::uint16_t >( defered_conn_event_counter_ - conn_event_counter_ ) & 0x8000 )
                {
                    result = ll_result::disconnect;
                }
                else
                {
                    defered_ll_control_pdu_ = pdu;
                }
            }
            else if ( opcode == LL_PING_REQ && size == 1 )
            {
                write.fill( { ll_control_pdu_code, 1, LL_PING_RSP } );
            }
            else if ( opcode == LL_FEATURE_REQ && size == 9 )
            {
                static constexpr std::uint8_t LE_PING_FEATURE_FLAG = 0x10;

                write.fill( {
                    ll_control_pdu_code, 9,
                    LL_FEATURE_RSP,
                    LE_PING_FEATURE_FLAG,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                } );
            }
            else
            {
                write.fill( { ll_control_pdu_code, 2, LL_UNKNOWN_RSP, opcode } );
            }

            if ( commit )
                this->commit_transmit_buffer( write );
        }

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_l2cap( const write_buffer& input )
    {
        const std::uint8_t  pdu_size      = input.buffer[ 1 ];
        const std::uint16_t l2cap_size    = read_16( &input.buffer[ 2 ] );
        const std::uint16_t l2cap_channel = read_16( &input.buffer[ 4 ] );

        if ( pdu_size - l2cap_header_size != l2cap_size )
            return ll_result::disconnect;

        auto output = this->allocate_transmit_buffer();

        if ( output.empty() )
            return ll_result::go_ahead;

        if ( l2cap_channel == l2cap_gap_channel )
        {
            std::size_t gap_size = output.size - all_header_size;

            server_->l2cap_input( &input.buffer[ all_header_size ], l2cap_size, &output.buffer[ all_header_size ], gap_size, connection_details_ );

            output.buffer[ 0 ] = lld_data_pdu_code;
            output.buffer[ 1 ] = static_cast< std::uint8_t >( gap_size + l2cap_header_size );
            output.buffer[ 2 ] = static_cast< std::uint8_t >( gap_size );
            output.buffer[ 3 ] = 0;
            output.buffer[ 4 ] = static_cast< std::uint8_t >( l2cap_gap_channel );
            output.buffer[ 5 ] = static_cast< std::uint8_t >( l2cap_gap_channel >> 8 );

            this->commit_transmit_buffer( output );
        }
        else if ( l2cap_channel == l2cap_sm_channel )
        {
            std::size_t sm_size = output.size - all_header_size;

            static_cast< security_manager_t& >( *this ).l2cap_input( &input.buffer[ all_header_size ], l2cap_size, &output.buffer[ all_header_size ], sm_size );

            output.buffer[ 0 ] = lld_data_pdu_code;
            output.buffer[ 1 ] = static_cast< std::uint8_t >( sm_size + l2cap_header_size );
            output.buffer[ 2 ] = static_cast< std::uint8_t >( sm_size );
            output.buffer[ 3 ] = 0;
            output.buffer[ 4 ] = static_cast< std::uint8_t >( l2cap_sm_channel );
            output.buffer[ 5 ] = static_cast< std::uint8_t >( l2cap_sm_channel >> 8 );

            this->commit_transmit_buffer( output );
        }

        return ll_result::go_ahead;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    typename link_layer< Server, ScheduledRadio, Options... >::ll_result link_layer< Server, ScheduledRadio, Options... >::handle_pending_ll_control()
    {
        ll_result result = ll_result::go_ahead;

        if ( !defered_ll_control_pdu_.empty() && defered_conn_event_counter_ == conn_event_counter_ )
        {
            const std::uint8_t opcode = defered_ll_control_pdu_.buffer[ 2 ];

            if ( opcode == LL_CHANNEL_MAP_REQ )
            {
                channels_.reset( &defered_ll_control_pdu_.buffer[ 3 ] );
            }
            else if ( opcode == LL_CONNECTION_UPDATE_REQ )
            {
                connection_interval_old_ = connection_interval_;
                if ( parse_timing_parameters_from_connection_update_request( defered_ll_control_pdu_ ) )
                {
                    timeouts_til_connection_lost_ = 0;
                    state_ = state::connection_update;
                }
                else
                {
                    result = ll_result::disconnect;
                }
            }
            else
            {
                assert( !"invalid opcode" );
            }

            defered_ll_control_pdu_ = write_buffer{ nullptr, 0 };
        }

        return result;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint16_t link_layer< Server, ScheduledRadio, Options... >::read_16( const std::uint8_t* p )
    {
        return *p | *( p + 1 ) << 8;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint32_t link_layer< Server, ScheduledRadio, Options... >::read_24( const std::uint8_t* p )
    {
        return static_cast< std::uint32_t >( read_16( p ) ) | static_cast< std::uint32_t >( *( p + 2 ) ) << 16;
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    std::uint32_t link_layer< Server, ScheduledRadio, Options... >::read_32( const std::uint8_t* p )
    {
        return static_cast< std::uint32_t >( read_16( p ) ) | static_cast< std::uint32_t >( read_16( p + 2 ) ) << 16;
    }

}
}

#endif
