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

bicycle gatt;

void callback()
{
    gatt.csc_wheel_revolution( 42,43 );
}

int main()
{
    bluetoe::nrf51< bicycle > server;

    for ( ;; )
        server.run( gatt );
}
