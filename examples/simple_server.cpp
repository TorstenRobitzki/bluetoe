
#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>
#include <cstdint>

std::int32_t temperature;

const char serivce_name[]        = "Temperature / Bathroom";
const char characteristic_name[] = "Temperature in 10th degree celsius";

typedef bluetoe::server<
    bluetoe::service_name< serivce_name >,
    bluetoe::characteristic<
        bluetoe::characteristic_name< characteristic_name >,
        bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
        bluetoe::read_only
    >
> temperature_service;

int main()
{
    temperature_service server;

    // Binding to L2CAP
    static_cast< void >( server );
}