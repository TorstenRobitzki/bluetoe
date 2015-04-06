
#include <bluetoe.hpp>
#include <cstdtype>

std::int32_t temperature;

typedef bluetoe::server<
    bluetoe::service_name< "Temperature / Bathroom" >,
    bluetoe::characteristics<
        bluetoe::characteristic<
            bluetoe::characteristic_name< "Temperature" >,
            bluetoe::bind_characteristic_value< &temperature >
        >
    >
> temperature_service;

int main()
{
    temperature_service server;
}