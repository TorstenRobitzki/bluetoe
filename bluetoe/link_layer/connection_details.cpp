#include <bluetoe/link_layer/connection_details.hpp>

namespace bluetoe {
namespace link_layer {

    connection_details::connection_details(
        const address& remote, const address& local, const channel_map& channels,
        std::uint16_t inter, std::uint16_t lat, std::uint16_t to,
        unsigned acc )
        : local_addr_( local )
        , remote_addr_( remote )
        , channels_( channels )
        , interval_( inter )
        , latency_( lat )
        , timeout_( to )
        , accuracy_( acc )
    {
    }

    const address& connection_details::remote_address() const
    {
        return remote_addr_;
    }

    const address& connection_details::local_address() const
    {
        return local_addr_;
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



}
}