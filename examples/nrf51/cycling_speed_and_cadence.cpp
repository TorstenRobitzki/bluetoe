#include <bluetoe/bindings/nrf51.hpp>
#include <bluetoe/services/csc.hpp>
#include <bluetoe/server.hpp>

static constexpr char server_name[] = "Gruener Blitz";

typedef bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::cycling_speed_and_cadence<
        bluetoe::csc::wheel_revolution_data_supported
    >
> bicycle;

int main()
{
    bicycle                   gatt;
    bluetoe::nrf51< bicycle > server;

    for ( ;; )
        server.run( gatt );
}
