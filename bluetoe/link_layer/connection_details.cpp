#include <connection_details.hpp>

namespace bluetoe {
namespace link_layer {

    connection_details::connection_details(
        const channel_map& channels,
        std::uint16_t inter, std::uint16_t lat, std::uint16_t to,
        unsigned acc )
        : channels_( channels )
        , interval_( inter )
        , latency_( lat )
        , timeout_( to )
        , accuracy_( acc )
    {
    }

    const channel_map& connection_details::channels() const
    {
        return channels_;
    }

    std::uint16_t connection_details::interval() const
    {
        return interval_;
    }

    std::uint16_t connection_details::latency() const
    {
        return latency_;
    }

    std::uint16_t connection_details::timeout() const
    {
        return timeout_;
    }

    unsigned connection_details::cumulated_sleep_clock_accuracy_ppm() const
    {
        return accuracy_;
    }


    connection_addresses::connection_addresses( const device_address& local, const device_address& remote )
        : local_addr_( local )
        , remote_addr_( remote )
    {
    }

    const device_address& connection_addresses::remote_address() const
    {
        return remote_addr_;
    }

    const device_address& connection_addresses::local_address() const
    {
        return local_addr_;
    }
}
}
