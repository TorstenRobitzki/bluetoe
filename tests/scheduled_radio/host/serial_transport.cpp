#include "host/serial_transport.hpp"

#include "host/errors.hpp"
#include "host/stream_transport.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/system/system_error.hpp>

namespace bluetoe {
namespace test_rig {

    struct serial_transport::impl
    {
        /*
         * Asio opens the port in raw mode; the baud rate is set because a port wants
         * one, USB CDC ignores it.
         */
        impl( const std::string& device, std::chrono::milliseconds timeout )
            : port( io )
            , transport( io, port, device, timeout )
        {
            try
            {
                port.open( device );
                port.set_option( boost::asio::serial_port::baud_rate( 115200 ) );
            }
            catch ( const boost::system::system_error& error )
            {
                throw rig_error( "cannot open " + device + ": " + error.what() );
            }
        }

        boost::asio::io_context                             io;
        boost::asio::serial_port                            port;
        stream_transport< boost::asio::serial_port >        transport;
    };

    serial_transport::serial_transport( const std::string& device, std::chrono::milliseconds timeout )
        : impl_( std::make_unique< impl >( device, timeout ) )
    {
    }

    serial_transport::~serial_transport() = default;

    std::vector< std::uint8_t > serial_transport::transact( std::span< const std::uint8_t > request )
    {
        return impl_->transport.transact( request );
    }
}
}
