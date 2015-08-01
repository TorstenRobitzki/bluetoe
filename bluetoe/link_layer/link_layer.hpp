#ifndef BLUETOE_LINK_LAYER_LINK_LAYER_HPP
#define BLUETOE_LINK_LAYER_LINK_LAYER_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/link_layer/address.hpp>
#include <bluetoe/link_layer/channel_map.hpp>
#include <bluetoe/options.hpp>

#include <algorithm>
#include <cassert>

namespace bluetoe {
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
    }

    /**
     * @brief link layer implementation
     *
     * Implements a binding to a server by implementing a link layout on top of a ScheduleRadio device.
     *
     * Missing Parameters:
     * - Advertising Interval
     * - opt. Address
     * - opt. Advertising Event type
     * - opt. Used Channels
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
    class link_layer : public ScheduledRadio< details::buffer_sizes< Options... >::tx_size, details::buffer_sizes< Options... >::rx_size, link_layer< Server, ScheduledRadio, Options... > >
    {
    public:
        link_layer();

        void run( Server& );

        void received( const read_buffer& receive );

        void timeout();

        void crc_error();

    private:
        typedef ScheduledRadio<
            details::buffer_sizes< Options... >::tx_size,
            details::buffer_sizes< Options... >::rx_size,
            link_layer< Server, ScheduledRadio, Options... > > radio_t;

        // calculates the time point for the next advertising event
        delta_time next_adv_event();

        void fill_advertising_buffer( const Server& server );
        void fill_advertising_response_buffer( const Server& server );
        bool is_valid_scan_request( const read_buffer& receive ) const;
        bool is_valid_connect_request( const read_buffer& receive ) const;
        unsigned sleep_clock_accuracy( const read_buffer& receive ) const;
        bool parse_transmit_window_from_connect_request( const read_buffer& valid_connect_request );

        std::uint8_t* advertising_buffer();
        std::uint8_t* advertising_response_buffer();
        std::uint8_t* advertising_receive_buffer();

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

        static constexpr std::uint8_t   header_txaddr_field         = 0x40;

        static constexpr std::uint32_t  advertising_radio_access_address = 0x8E89BED6;
        static constexpr std::uint32_t  advertising_crc_init             = 0x555555;

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
        unsigned                        timeouts_til_connection_lost_;

        enum class state
        {
            initial,
            advertising,
            connecting,
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
        , state_( state::initial )
    {
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::run( Server& server )
    {
        // after the initial scheduling, the timeout and receive callback will setup the next scheduling
        if ( state_ == state::initial )
        {
            state_ = state::advertising;
            fill_advertising_buffer( server );
            fill_advertising_response_buffer( server );

            this->set_access_address_and_crc_init(
                advertising_radio_access_address,
                advertising_crc_init );

            this->schedule_advertisment_and_receive(
                current_channel_index_,
                write_buffer{ advertising_buffer(), adv_size_ }, delta_time::now(),
                read_buffer{ advertising_receive_buffer(), maximum_adv_request_size } );
        }

        radio_t::run();
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::received( const read_buffer& receive )
    {
        if ( state_ == state::advertising )
        {
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
                && parse_transmit_window_from_connect_request( receive ) )
            {
                state_                          = state::connecting;
                current_channel_index_    = 0;
                cumulated_sleep_clock_accuracy_ = sleep_clock_accuracy( receive ) + device_sleep_clock_accuracy::accuracy_ppm;
                timeouts_til_connection_lost_   = num_windows_til_timeout - 1;

                this->set_access_address_and_crc_init(
                    read_32( &receive.buffer[ 14 ] ),
                    read_24( &receive.buffer[ 18 ] ) );

                const delta_time window_start = transmit_window_offset_ - transmit_window_offset_.ppm( cumulated_sleep_clock_accuracy_ );
                      delta_time window_end   = transmit_window_offset_ + transmit_window_size_;

                window_end += window_end.ppm( cumulated_sleep_clock_accuracy_ );

                this->schedule_connection_event(
                    channels_.data_channel( current_channel_index_ ),
                    window_start,
                    window_end,
                    connection_interval_ );
            }
            else
            {
                timeout();
            }
        }
        else if ( state_ == state::connecting )
        {
            timeout();
        }
        else
        {
            assert( !"invalid state" );
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::timeout()
    {
        if ( state_ == state::advertising )
        {
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
        else if ( state_ == state::connecting )
        {
            if ( timeouts_til_connection_lost_ )
            {
                ++current_channel_index_;

                const delta_time con_interval_offset = connection_interval_ * ( num_windows_til_timeout - timeouts_til_connection_lost_ );
                --timeouts_til_connection_lost_;

                delta_time window_start = transmit_window_offset_ + con_interval_offset;
                delta_time window_end   = window_start + transmit_window_size_;

                window_start -= window_start.ppm( cumulated_sleep_clock_accuracy_ );
                window_end   += window_end.ppm( cumulated_sleep_clock_accuracy_ );

                this->schedule_connection_event(
                    channels_.data_channel( current_channel_index_ ),
                    window_start,
                    window_end,
                    connection_interval_ );
            }
            else
            {
                current_channel_index_ = last_advertising_channel;
                state_ = state::advertising;

                timeout();
            }
        }
        else
        {
            assert( !"invalid state" );
        }
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::crc_error()
    {
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
    void link_layer< Server, ScheduledRadio, Options... >::fill_advertising_buffer( const Server& server )
    {
        std::uint8_t* const adv_buffer = advertising_buffer();

        adv_buffer[ 0 ] = adv_ind_pdu_type_code;

        if ( device_address::is_random() )
            adv_buffer[ 0 ] |= header_txaddr_field;

        adv_buffer[ 1 ] = address_length + server.advertising_data( &adv_buffer[ advertising_pdu_header_size + address_length ], max_advertising_data_size );
        std::copy( address_.begin(), address_.end(), &adv_buffer[ 2 ] );
        adv_size_ = advertising_pdu_header_size + adv_buffer[ 1 ];
    }

    template < class Server, template < std::size_t, std::size_t, class > class ScheduledRadio, typename ... Options >
    void link_layer< Server, ScheduledRadio, Options... >::fill_advertising_response_buffer( const Server& server )
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
    bool link_layer< Server, ScheduledRadio, Options... >::parse_transmit_window_from_connect_request( const read_buffer& valid_connect_request )
    {
        static constexpr delta_time maximum_transmit_window_offset( 10 * 1000 );
        static constexpr auto       us_per_digits = 1250;

        transmit_window_size_   = delta_time( valid_connect_request.buffer[ 21 ] * us_per_digits );
        transmit_window_offset_ = delta_time( read_16( &valid_connect_request.buffer[ 22 ] ) * us_per_digits + us_per_digits );
        connection_interval_    = delta_time( read_16( &valid_connect_request.buffer[ 24 ] ) * us_per_digits );

        bool result = transmit_window_size_ <= maximum_transmit_window_offset
                   && transmit_window_size_ <= connection_interval_
                   && transmit_window_offset_ <= connection_interval_;

        return result;
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
