#ifndef BLUETOE_LINK_LAYER_L2CAP_SIGNALING_CHANNEL_HPP
#define BLUETOE_LINK_LAYER_L2CAP_SIGNALING_CHANNEL_HPP

#include <cstdlib>
#include <cstdint>

namespace bluetoe {
namespace l2cap {

    template < typename ... Options >
    class signaling_channel
    {
    public:
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        void l2cap_output( std::uint8_t* output, std::size_t& out_size );

        void connection_parameter_update_request( std::uint16_t interval_min, std::uint16_t interval_max, std::uint16_t latency, std::uint16_t timeout );

    private:
        void reject_command( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        static constexpr std::uint8_t command_reject_code = 0x01;
    };

    template < typename ... Options >
    void signaling_channel< Options... >::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        reject_command( input, in_size, output, out_size );
    }

    template < typename ... Options >
    void signaling_channel< Options... >::l2cap_output( std::uint8_t* output, std::size_t& out_size )
    {
        out_size = 0;
    }

    template < typename ... Options >
    void signaling_channel< Options... >::connection_parameter_update_request( std::uint16_t interval_min, std::uint16_t interval_max, std::uint16_t latency, std::uint16_t timeout )
    {
    }

    template < typename ... Options >
    void signaling_channel< Options... >::reject_command( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        static constexpr std::size_t    pdu_size = 6;
        static constexpr std::uint16_t  command_not_understood = 0;
        assert( out_size >= pdu_size );

        const std::uint8_t identifier = in_size >= 2 ? input[ 1 ] : 0x00;

        out_size = pdu_size;
        output[ 0 ] = command_reject_code;
        output[ 1 ] = identifier;
        output[ 2 ] = 2;                    // length
        output[ 3 ] = 0;                    // length
        output[ 4 ] = static_cast< std::uint8_t >( command_not_understood );
        output[ 5 ] = static_cast< std::uint8_t >( command_not_understood >> 8 );
    }
}
}

#endif
