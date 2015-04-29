# Bluetoe

## Overview

Bluetoe implements a GATT server with a very low memory footprint and a convenience C++ interface. Bluetoe makes easy things easy but gives the opportunity to fiddle with all the low level GATT details if necessary. The main target of Bluetoe is to be implemented on very small microcontrollers. Here is an example of a small GATT server:

    #include <bluetoe/server.hpp>
    #include <bluetoe/service.hpp>
    #include <bluetoe/characteristic.hpp>

    std::int32_t temperature;
    const char serivce_name[]        = "Temperature / Bathroom";
    const char characteristic_name[] = "Temperature in 10th degree celsius";

    typedef bluetoe::server<
        bluetoe::server_name< server_name >,
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0000, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::characteristic_name< char_name >,
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0000, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
                bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >
            >
        >
    > temperature_service;

    int main()
    {
        temperature_service                                             server;
        bluetoe::binding::btstack_libusb_device< temperature_service >  device;

        device.run( server );
    }

## L2CAP

Bluetoe adds on top of an existing L2CAP implementation. Currently it comes only with one experimental L2CAP implementation on top of btstack's HCI layer that runs at least on OS/X but should also work on Linux (and maybe Windows).

## Current State

The following table show the list of GATT procedures and there implementation status and there planned implementation status:

Feature | Sub-Procedure | Status
--------|---------------|-------
Server Configuration|Exchange MTU|implemented
Primary Service Discovery|Discover All Primary Services|implemented
 |Discover Primary Service By Service UUID|implemented
Relationship Discovery|Find Included Services|implemented
Characteristic Discovery|Discover All Characteristic of a Service|implemented
 |Discover Characteristic by UUID|implemented
Characteristic Descriptor Discovery|Discover All Characteristic Descriptors|implemented
Characteristic Value Read|Read Characteristic Value|implemented
 |Read Using Characteristic UUID|implemented
 |Read Long Characteristic Value|implemented
 |Read Multiple Characteristic Values|planned
Characteristic Value Write| Write Without Response|planned
 |Signed Write Without Response|not planned
 |Write Characteristic Value|implemented
 |Write Long Characteristic Values|planned
 |Characteristic Value Reliable Writes|planned
Characteristic Value Notification|Notifications|implemented
Characteristic Value Indication|Indications|planned
Characteristic Descriptor Value Read|Read Characteristic Descriptors|planned
 |Read Long Characteristic Descriptors|planned
Characteristic Descriptor Value Write|Write Characteristic Descriptors|planned
 |Write Long Characteristic Descriptors|planned
Cryptography|Encryption|not planned
 |Authentication|not planned

This is the current state of implemented Advertising Data:

Advertising Data|Format|Status
----------------|------|------
Service UUID|Incomplete List of 16-bit Service UUIDs|planned
 |Complete List of 16-bit Service UUIDs|planned
 |Incomplete List of 32-bit Service UUIDs|planned
 |Complete List of 32-bit Service UUIDs|planned
 |Incomplete List of 128-bit Service UUIDs|planned
 |Complete List of 128-bit Service UUIDs|planned
Local Name|Shortened Local Name|implemented
 |Complete Local Name|implemented
Flags|Flags|planned
Manufacturer Specific Data|Manufacturer Specific Data|planned
TX Power Level|TX Power Level|planned
Secure Simple Pairing Out of Band||not planned
Security Manager Out of Band||not planned
Security Manager TK Value||not planned
Slave Connection Interval Range||not planned
Service Solicitation||not planned
Service Data||not planned
Appearance|Appearance|planned
LE Role|LE Role|planned

Pullrequests are wellcome.

## Dependencies
- boost for Unittests
- CMake for build
- a decent C++ compiler supporting C++11
- btstack for experimental L2CAP support
