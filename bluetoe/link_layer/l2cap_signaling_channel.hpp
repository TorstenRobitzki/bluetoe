#ifndef BLUETOE_LINK_LAYER_L2CAP_SIGNALING_CHANNEL_HPP
#define BLUETOE_LINK_LAYER_L2CAP_SIGNALING_CHANNEL_HPP

#include <cstdlib>
#include <cstdint>

namespace bluetoe {

namespace details {
    struct signaling_channel_meta_type {};
}

namespace l2cap {

    /**
     * @brief very basic l2cap signaling channel implementation
     *
     * Currently the implementation allows just for sending connection parameter update requests.
     */
    template < typename ... Options >
    class signaling_channel
    {
    public:
        signaling_channel();

        /**
         * @brief input from the l2cap layer
         */
        void signaling_channel_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        /**
         * @brief output to the l2cap layer
         */
        void signaling_channel_output( std::uint8_t* output, std::size_t& out_size );

        /**
         * @brief queues a connection parameter update request.
         *
         * Function returns true on success. If the a request is still queued, but was not responded jet,
         * the function will return false.
         */
        bool connection_parameter_update_request( std::uint16_t interval_min, std::uint16_t interval_max, std::uint16_t latency, std::uint16_t timeout );

        /** @cond HIDDEN_SYMBOLS */
        typedef bluetoe::details::signaling_channel_meta_type meta_type;
        /** @endcond */
    private:
        void reject_command( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        static constexpr std::uint8_t command_reject_code                       = 0x01;
        static constexpr std::uint8_t connection_parameter_update_request_code  = 0x12;
        static constexpr std::uint8_t connection_parameter_update_response_code = 0x13;

        std::uint16_t interval_min_;
        std::uint16_t interval_max_;
        std::uint16_t latency_;
        std::uint16_t timeout_;

        enum {
            idle,
            queued,
            transmitted
        } pending_status_;

        static constexpr std::uint8_t   invalid_identifier = 0x00;
        std::uint8_t identifier_;
    };

    /**
     * @brief signaling channel implementations that does simply nothing
     */
    class no_signaling_channel
    {
    public:
        /**
         * @copydoc signaling_channel::signaling_channel_input
         */
        void signaling_channel_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
        {
            out_size = 0;
        }

        /**
         * @copydoc signaling_channel::signaling_channel_output
         */
        void signaling_channel_output( std::uint8_t* output, std::size_t& out_size )
        {
            out_size = 0;
        }

        /**
         * @copydoc signaling_channel::connection_parameter_update_request
         */
        bool connection_parameter_update_request( std::uint16_t interval_min, std::uint16_t interval_max, std::uint16_t latency, std::uint16_t timeout )
        {
            return false;
        }

        /** @cond HIDDEN_SYMBOLS */
        typedef bluetoe::details::signaling_channel_meta_type meta_type;
        /** @endcond */
    };

    template < typename ... Options >
    signaling_channel< Options... >::signaling_channel()
        : pending_status_( idle )
        , identifier_( 0x01 )
    {
    }

    template < typename ... Options >
    void signaling_channel< Options... >::signaling_channel_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        const std::uint8_t code = in_size > 0 ? input[ 0 ] : 0;

        if ( code == connection_parameter_update_response_code && pending_status_ == transmitted )
        {
            pending_status_ = idle;
            identifier_ = static_cast< std::uint8_t >( identifier_ + 1 );

            if ( identifier_ == invalid_identifier )
                identifier_ = static_cast< std::uint8_t >( identifier_ + 1 );

            out_size = 0;
        }
        else
        {
            reject_command( input, in_size, output, out_size );
        }
    }

    template < typename ... Options >
    void signaling_channel< Options... >::signaling_channel_output( std::uint8_t* output, std::size_t& out_size )
    {
        static constexpr std::size_t    pdu_size = 8 + 4;
        assert( out_size >= pdu_size );

        if ( pending_status_ == queued )
        {
            pending_status_ = transmitted;

            out_size = pdu_size;
            output[ 0 ] = connection_parameter_update_request_code;
            output[ 1 ] = identifier_;
            output[ 2 ] = pdu_size - 4;
            output[ 3 ] = 0;
            output[ 4 ] = static_cast< std::uint8_t >( interval_min_ );
            output[ 5 ] = static_cast< std::uint8_t >( interval_min_ >> 8 );
            output[ 6 ] = static_cast< std::uint8_t >( interval_max_ );
            output[ 7 ] = static_cast< std::uint8_t >( interval_max_ >> 8 );
            output[ 8 ] = static_cast< std::uint8_t >( latency_ );
            output[ 9 ] = static_cast< std::uint8_t >( latency_ >> 8 );
            output[ 10 ] = static_cast< std::uint8_t >( timeout_ );
            output[ 11 ] = static_cast< std::uint8_t >( timeout_ >> 8 );
        }
        else
        {
            out_size = 0;
        }
    }

    template < typename ... Options >
    bool signaling_channel< Options... >::connection_parameter_update_request( std::uint16_t interval_min, std::uint16_t interval_max, std::uint16_t latency, std::uint16_t timeout )
    {
        if ( pending_status_ == idle )
        {
            interval_min_ = interval_min;
            interval_max_ = interval_max;
            latency_      = latency;
            timeout_      = timeout;

            pending_status_ = queued;
            return true;
        }

        return false;
    }

    template < typename ... Options >
    void signaling_channel< Options... >::reject_command( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        static constexpr std::size_t    pdu_size = 6;
        static constexpr std::uint16_t  command_not_understood = 0;
        assert( out_size >= pdu_size );
        out_size = 0;

        if ( in_size < 2 )
            return;

        const std::uint8_t identifier = input[ 1 ];

        if ( identifier == invalid_identifier )
            return;

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
