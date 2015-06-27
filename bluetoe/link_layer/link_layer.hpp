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
     */
    template < class Server, template < class CallBack > class ScheduledRadio >
    class link_layer : public ScheduledRadio< link_layer< Server, ScheduledRadio > >
    {
    public:
        void run( Server& );

        void received( const read_buffer& receive );

        void timeout();
    private:
        static constexpr std::size_t    max_advertising_data_size   = 31;
        static constexpr std::size_t    advertising_pdu_header_size = 2;
    };

    // implementation
    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::run( Server& server )
    {
        static std::uint8_t buffer[ max_advertising_data_size + advertising_pdu_header_size ];
        const write_buffer out{ buffer, server.advertising_data( buffer, sizeof( buffer ) ) };

        this->schedule_transmit_and_receive(
            37,
            out, delta_time::now(),
            read_buffer(), delta_time::usec( 10u ) );
    }

    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::received( const read_buffer& receive )
    {
    }

    template < class Server, template < class > class ScheduledRadio >
    void link_layer< Server, ScheduledRadio >::timeout()
    {
    }

}
}

#endif
