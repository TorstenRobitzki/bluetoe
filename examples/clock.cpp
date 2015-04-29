#include <iostream>
#include <thread>
#include <chrono>
#include <ctime>
#include <bluetoe/server.hpp>
#include <bluetoe/bindings/btstack_libusb.hpp>

namespace bt = bluetoe;

char clock_text[ 9 ] = "HH:MM:SS";
char current_time[]  = "current time";
char time_service[]  = "Time Service";

typedef bt::server<
    bt::service<
        bt::service_uuid< 0xB00B736E, 0x9A4E, 0x40DE, 0xA892, 0x1E163ADECEC8 >,
        bt::service_name< time_service >,
        bt::characteristic<
            bt::characteristic_uuid< 0x47EEED4E, 0xBDC3, 0x467C, 0x9370, 0x9F079D8A5A6B >,
            bt::characteristic_name< current_time >,
            bt::bind_characteristic_value< decltype( clock_text ), &clock_text >,
            bt::no_write_access,
            bt::notify
        >
    >
> server_type;

server_type server;

void update_clock()
{
    for ( ;; )
    {
        auto        now  = std::chrono::system_clock::now();
        std::time_t cnow = std::chrono::system_clock::to_time_t( now );

        std::strftime( clock_text, sizeof( clock_text ), "%H:%M:%S", std::localtime( &cnow ) );
        server.notify( clock_text );

        std::cout << clock_text << std::endl;

        std::this_thread::sleep_until( now + std::chrono::seconds( 1 ) );
    }
}

int main()
{
    std::thread update( update_clock );

    bluetoe::binding::btstack_libusb_device< server_type >  device;

    device.run( server );
}