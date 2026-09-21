#include <bluetoe/connection_parameters.hpp>
#include <bluetoe/bits.hpp>
#include <bluetoe/peripheral_latency.hpp>

namespace bluetoe {
namespace link_layer {
namespace details {

    namespace {
        // the unit of the interval, the transmit window and its offset on the air
        constexpr unsigned us_per_digits = 1250;

        // the central's sleep clock accuracy, from the three bits of a CONNECT_IND (Vol 6, Part B, 2.3.3.1)
        unsigned central_sleep_clock_accuracy_ppm( const std::uint8_t* body )
        {
            static constexpr std::uint16_t inaccuracy_ppm[ 8 ] = {
                500, 250, 150, 100, 75, 50, 30, 20
            };

            return inaccuracy_ppm[ ( body[ 33 ] >> 5 ) & 0x7 ];
        }
    }

    bool connection_parameters::from_connect_request( const std::uint8_t* body, unsigned local_sleep_clock_accuracy_ppm )
    {
        using namespace ::bluetoe::details;

        if ( !channels_.reset( &body[ 28 ], body[ 33 ] & 0x1f ) )
            return false;

        const delta_time transmit_window_offset = delta_time( read_16bit( &body[ 20 ] ) * us_per_digits );

        transmit_window_size_           = delta_time( body[ 19 ] * us_per_digits );
        transmit_window_offset_         = delta_time( read_16bit( &body[ 20 ] ) * us_per_digits + us_per_digits );
        interval_                       = delta_time( read_16bit( &body[ 22 ] ) * us_per_digits );
        latency_                        = read_16bit( &body[ 24 ] );
        timeout_value_                  = read_16bit( &body[ 26 ] );
        timeout_                        = delta_time( timeout_value_ * 10000 );
        cumulated_sleep_clock_accuracy_ = central_sleep_clock_accuracy_ppm( body ) + local_sleep_clock_accuracy_ppm;

        return transmit_window_offset <= interval_ && valid();
    }

    bool connection_parameters::from_connection_update( const std::uint8_t* body )
    {
        using namespace ::bluetoe::details;

        transmit_window_size_   = delta_time( body[ 1 ] * us_per_digits );
        transmit_window_offset_ = delta_time( read_16bit( &body[ 2 ] ) * us_per_digits );
        interval_               = delta_time( read_16bit( &body[ 4 ] ) * us_per_digits );
        latency_                = read_16bit( &body[ 6 ] );
        timeout_value_          = read_16bit( &body[ 8 ] );
        timeout_                = delta_time( timeout_value_ * 10000 );

        return transmit_window_offset_ <= interval_ && valid();
    }

    void connection_parameters::channels( const std::uint8_t* map )
    {
        channels_.reset( map );
    }

    const channel_map& connection_parameters::channels() const
    {
        return channels_;
    }

    delta_time connection_parameters::interval() const
    {
        return interval_;
    }

    std::uint16_t connection_parameters::latency() const
    {
        return latency_;
    }

    delta_time connection_parameters::timeout() const
    {
        return timeout_;
    }

    unsigned connection_parameters::sleep_clock_accuracy_ppm() const
    {
        return cumulated_sleep_clock_accuracy_;
    }

    bool connection_parameters::transmit_window_pending() const
    {
        return !transmit_window_size_.zero();
    }

    delta_time connection_parameters::transmit_window_offset() const
    {
        return transmit_window_offset_;
    }

    delta_time connection_parameters::transmit_window_size() const
    {
        return transmit_window_size_;
    }

    void connection_parameters::transmit_window_passed()
    {
        transmit_window_size_ = delta_time();
    }

    connection_details connection_parameters::details() const
    {
        return connection_details(
            channels_,
            interval_.usec() / us_per_digits,
            latency_,
            timeout_value_,
            cumulated_sleep_clock_accuracy_ );
    }

    bool connection_parameters::valid() const
    {
        static constexpr delta_time maximum_transmit_window_offset( 10 * 1000 );
        static constexpr delta_time maximum_connection_timeout( 32 * 1000 * 1000 );
        static constexpr delta_time minimum_connection_timeout( 100 * 1000 );

        return transmit_window_size_ <= maximum_transmit_window_offset
            && transmit_window_size_ <= interval_
            && timeout_ >= minimum_connection_timeout
            && timeout_ <= maximum_connection_timeout
            && timeout_ >= ( latency_ + 1 ) * 2 * interval_
            && latency_ <= maximum_link_layer_peripheral_latency;
    }
}
}
}
