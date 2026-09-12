#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_SERIAL_TRANSPORT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_SERIAL_TRANSPORT_HPP

/**
 * @file serial_transport.hpp
 *
 * The host's end of the link to a real instrument: stream_transport.hpp over a serial
 * port. Boost.Asio stays inside serial_transport.cpp, so that including this costs
 * nothing.
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace bluetoe {
namespace test_rig {

    class serial_transport
    {
    public:
        /**
         * @brief opens the device; the timeout is that of stream_transport
         *
         * @throws rig_error the device cannot be opened
         */
        serial_transport( const std::string& device, std::chrono::milliseconds timeout );
        ~serial_transport();

        /**
         * @brief stream_transport::transact() on the port
         */
        std::vector< std::uint8_t > transact( std::span< const std::uint8_t > request );

    private:
        struct impl;
        std::unique_ptr< impl > impl_;
    };
}
}

#endif
