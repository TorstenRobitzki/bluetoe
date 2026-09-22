#ifndef BLUETOE_LINK_LAYER_CONNECTION_PARAMETERS_HPP
#define BLUETOE_LINK_LAYER_CONNECTION_PARAMETERS_HPP

#include <bluetoe/channel_map.hpp>
#include <bluetoe/connection_details.hpp>
#include <bluetoe/delta_time.hpp>

#include <cstdint>

namespace bluetoe {
namespace link_layer {
namespace details {

    /**
     * @brief the timing of a connection and its channel map: what a CONNECT_IND set, and
     *        what an LL_CONNECTION_UPDATE_IND or an LL_CHANNEL_MAP_REQ changes at its instant
     *
     * What a request changes is parsed out of line; what every connection event asks for is
     * defined here, because a call per parameter and per event is Flash the link layer did
     * not spend while these were its own members.
     */
    class connection_parameters
    {
    public:
        /**
         * @brief the parameters of a CONNECT_IND, `body` pointing at the PDU's payload
         *
         * `local_sleep_clock_accuracy_ppm` is this device's, added to the central's from the
         * PDU. False if the PDU does not describe a valid connection; the parameters are then
         * unusable until the next request.
         */
        bool from_connect_request( const std::uint8_t* body, unsigned local_sleep_clock_accuracy_ppm );

        /**
         * @brief the parameters of an LL_CONNECTION_UPDATE_IND, `body` pointing at its opcode
         *
         * False if they are invalid.
         */
        bool from_connection_update( const std::uint8_t* body );

        /**
         * @brief the channel map of an LL_CHANNEL_MAP_REQ, `map` pointing at its five bytes
         */
        void channels( const std::uint8_t* map );

        /**
         * @brief what the callbacks above the link layer are told about the connection
         */
        connection_details details() const;

        const channel_map& channels() const
        {
            return channels_;
        }

        delta_time interval() const
        {
            return interval_;
        }

        std::uint16_t latency() const
        {
            return latency_;
        }

        delta_time timeout() const
        {
            return timeout_;
        }

        unsigned sleep_clock_accuracy_ppm() const
        {
            return cumulated_sleep_clock_accuracy_;
        }

        /**
         * @brief whether the next connection event is the first after a request: placed in
         *        the transmit window the request named, not an interval after the anchor
         */
        bool transmit_window_pending() const
        {
            return !transmit_window_size_.zero();
        }

        delta_time transmit_window_offset() const
        {
            return transmit_window_offset_;
        }

        delta_time transmit_window_size() const
        {
            return transmit_window_size_;
        }

        void transmit_window_passed()
        {
            transmit_window_size_ = delta_time();
        }

    private:
        bool valid() const;

        channel_map     channels_;
        unsigned        cumulated_sleep_clock_accuracy_ = 0;
        delta_time      transmit_window_offset_;
        delta_time      transmit_window_size_;
        delta_time      interval_;
        std::uint16_t   latency_ = 0;
        std::uint16_t   timeout_value_ = 0;
        delta_time      timeout_;
    };
}
}
}

#endif
