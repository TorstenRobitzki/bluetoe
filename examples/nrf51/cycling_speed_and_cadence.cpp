#include <bluetoe/bindings/nrf51.hpp>
#include <bluetoe/services/csc.hpp>
#include <bluetoe/server.hpp>
#include <bluetoe/sensor_location.hpp>

static constexpr char server_name[] = "Gruener Blitz";

struct handler {

    std::pair< std::uint32_t, std::uint16_t > cumulative_wheel_revolutions_and_time();

    void set_cumulative_wheel_revolutions( std::uint32_t new_value );

    volatile std::uint32_t wheel_revolutions_;
    volatile std::uint16_t last_event_time_;
};

using bicycle = bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::cycling_speed_and_cadence<
        bluetoe::sensor_location::hip,
        bluetoe::csc::wheel_revolution_data_supported,
        bluetoe::csc::handler< handler >
    >
>;

bicycle gatt;

std::pair< std::uint32_t, std::uint16_t > handler::cumulative_wheel_revolutions_and_time()
{
    return std::make_pair( wheel_revolutions_, last_event_time_ );
}

void handler::set_cumulative_wheel_revolutions( std::uint32_t new_value )
{
    wheel_revolutions_ = new_value;
    gatt.confirm_cumulative_wheel_revolutions( gatt );
}

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
