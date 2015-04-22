# Bluetoe

## Overview

Bluetoe aims to implement a GATT server with a very low memory footprint and convenience C++ interface. Bluetoe tries to make easy things easy but gives the opportunity to fiddle with all the low level GATT details if necessary. Here is an example of a small GATT server:

    #include <bluetoe/server.hpp>
    #include <bluetoe/service.hpp>
    #include <bluetoe/characteristic.hpp>

    std::int32_t temperature;

    const char serivce_name[]        = "Temperature / Bathroom";
    const char characteristic_name[] = "Temperature in 10th degree celsius";

    typedef bluetoe::server<
        bluetoe::service_name< serivce_name >,
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_name< characteristic_name >,
            bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
            bluetoe::no_read_access
        >
    > temperature_service;

    int main()
    {
        temperature_service server;

        // Binding to L2CAP
        static_cast< void >( server );
    }
    
## L2CAP

Bluetoe adds on top of an existing L2CAP implementation. Currently it comes only with one experimental L2CAP implementation on top of btstack's HCI layer that runs at least on OS/X but should also work on Linux (and maybe Windows).

## Current State
The current state of the project is: `under construction`

Pullrequests are wellcome.

## Dependencies
- boost for Unittests
- CMake for build
- a decent C++ compiler supporting C++11
- btstack for experimental L2CAP support
