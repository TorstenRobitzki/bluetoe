#ifndef BLUETOE_LINK_LAYER_CONNECTION_DETAILS_HPP
#define BLUETOE_LINK_LAYER_CONNECTION_DETAILS_HPP

#include <bluetoe/link_layer/address.hpp>
#include <bluetoe/link_layer/channel_map.hpp>
#include <cstdint>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief data type to store details of an established link layer connection
     */
    class connection_details
    {
    public:
        /**
         * @brief default c'tor leaving all data members in a meaningless state
         */
        connection_details() = default;

        /**
         * @brief construct the data object from its required parts.
         */
        connection_details(
            const address& remote, const address& local, const channel_map& channels,
            std::uint16_t inter, std::uint16_t lat, std::uint16_t to,
            unsigned acc );

        /**
         * @brief remote device address
         *
         * The remote device address of the given connection.
         */
        const address& remote_address() const;

        /**
         * @brief local device address
         *
         * The local device address of the given connection.
         */
        const address& local_address() const;

        /**
         * @brief channels that a currently in use
         */
        const channel_map& channels() const;

        /**
         * @brief connection interval
         *
         * The connection interval of the current connection in units of 1.25ms.
         */
        std::uint16_t interval() const;

        /**
         * @brief slave latency
         *
         * The slave defined the number of connection events, the slave can
         * skip (do not respond).
         */
        std::uint16_t latency() const;

        /**
         * @brief connection timeout
         *
         * The connection timeout of the current connection in units of 10ms.
         */
        std::uint16_t timeout() const;

        /**
         * @brief the cumulated sleep clock accuracy in parts per million.
         *
         * The cumulated sleep clock accuracy is the sum of the clients and
         * slaves accuracy (clock error) and is taken into account for this
         * connection.
         */
        unsigned cumulated_sleep_clock_accuracy_ppm() const;

    private:
        address         local_addr_;
        address         remote_addr_;
        channel_map     channels_;

        std::uint16_t   interval_;
        std::uint16_t   latency_;
        std::uint16_t   timeout_;
        unsigned        accuracy_;
    };
}
}

#endif

