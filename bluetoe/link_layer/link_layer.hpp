#ifndef BLUETOE_LINK_LAYER_LINK_LAYER_HPP
#define BLUETOE_LINK_LAYER_LINK_LAYER_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>

namespace bluetoe {
namespace link_layer {


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
    template < class Server, template < class CallBack > class ScheduledRadio >
    class link_layer : public ScheduledRadio< link_layer< Server, ScheduledRadio > >
    {
    public:
        link_layer();

        void run( Server& );

        void received( const read_buffer& receive );

        void timeout();
    private:
        static constexpr std::size_t    max_advertising_data_size   = 31;
        static constexpr std::size_t    advertising_pdu_header_size = 2;

        static constexpr unsigned       first_advertising_channel   = 37;
        static constexpr unsigned       last_advertising_channel    = 39;

        unsigned                        current_advertising_channel_;
        std::uint8_t                    adv_buffer_[ max_advertising_data_size + advertising_pdu_header_size ];
        std::size_t                     adv_size_;
    };

    // implementation
    template < class Server, template < class > class ScheduledRadio >
    link_layer< Server, ScheduledRadio >::link_layer()
        : current_advertising_channel_( first_advertising_channel )
    {
    }

    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::run( Server& server )
    {
        adv_size_ = server.advertising_data( adv_buffer_, sizeof( adv_buffer_ ) );

        this->schedule_transmit_and_receive(
            current_advertising_channel_,
            write_buffer{ adv_buffer_, adv_size_ }, delta_time::now(),
            read_buffer(), delta_time::usec( 10u ) );

        ScheduledRadio< link_layer< Server, ScheduledRadio > >::run();
    }

    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::received( const read_buffer& receive )
    {
    }

    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::timeout()
    {
        current_advertising_channel_ = current_advertising_channel_ == last_advertising_channel
            ? first_advertising_channel
            : current_advertising_channel_ + 1;

        this->schedule_transmit_and_receive(
            current_advertising_channel_,
            write_buffer{ adv_buffer_, adv_size_ }, delta_time::now(),
            read_buffer(), delta_time::msec( 200 ) );
    }

}
}

#endif
