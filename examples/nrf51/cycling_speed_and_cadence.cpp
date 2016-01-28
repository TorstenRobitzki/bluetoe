#include <bluetoe/bindings/nrf51.hpp>
#include <bluetoe/services/csc.hpp>
#include <bluetoe/server.hpp>

static constexpr char server_name[] = "Gruener Blitz";

struct handler {

    std::pair< std::uint32_t, std::uint16_t > cumulative_wheel_revolutions()
    {
        return std::make_pair( wheel_revolutions_, last_event_time_ );
    }

    volatile std::uint32_t wheel_revolutions_;
    volatile std::uint16_t last_event_time_;
};

using bicycle = bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::cycling_speed_and_cadence<
        bluetoe::csc::wheel_revolution_data_supported,
        bluetoe::csc::handler< handler >
    >
>;

bicycle gatt;

void timer_callback()
{
    gatt.notify_timed_update( gatt );
}

bluetoe::nrf51< bicycle > server;

int main()
{
    for ( ;; )
        server.run( gatt );
}
