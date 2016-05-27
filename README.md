# Bluetoe [![Build Status](https://travis-ci.org/TorstenRobitzki/bluetoe.svg?branch=master)](https://travis-ci.org/TorstenRobitzki/bluetoe) [![Join the chat at https://gitter.im/TorstenRobitzki/bluetoe](https://badges.gitter.im/TorstenRobitzki/bluetoe.svg)](https://gitter.im/TorstenRobitzki/bluetoe?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

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

## Documentation

http://torstenrobitzki.github.io/bluetoe/

## L2CAP

Bluetoe ships with its own link layer. Currently a link layer based on the nrf51422 is under construction and is already usable. The link layer implementation will be based and tested on an abstract device, called a scheduled radio and should be easily ported to similar hardware. As Bluetoe is a GATT server implementation, only that parts of the link layer are implemented, that are nessary. Bluetoe can easily adapted to any other existing L2CAP implementation (based on HCI for example).

## Current State

The following table show the list of GATT procedures and there implementation status and there planned implementation status:

Feature | Sub-Procedure | Status
--------|---------------|-------
Server Configuration|Exchange MTU|implemented
Primary Service Discovery|Discover All Primary Services|implemented
 |Discover Primary Service By Service UUID|implemented
Relationship Discovery|Find Included Services|implemented
 |Declare Secondary Services|implemented
Characteristic Discovery|Discover All Characteristic of a Service|implemented
 |Discover Characteristic by UUID|implemented
Characteristic Descriptor Discovery|Discover All Characteristic Descriptors|implemented
Characteristic Value Read|Read Characteristic Value|implemented
 |Read Using Characteristic UUID|implemented
 |Read Long Characteristic Value|implemented
 |Read Multiple Characteristic Values|implemented
Characteristic Value Write| Write Without Response|implemented
 |Signed Write Without Response|not planned
 |Write Characteristic Value|implemented
 |Write Long Characteristic Values|implemented
 |Characteristic Value Reliable Writes|implemented
Characteristic Value Notification|Notifications|implemented
Characteristic Value Indication|Indications|implemented
Characteristic Descriptor Value Read|Read Characteristic Descriptors|implemented
 |Read Long Characteristic Descriptors|implemented
Characteristic Descriptor Value Write|Write Characteristic Descriptors|implemented
 |Write Long Characteristic Descriptors|implemented
Cryptography|Encryption|not planned
 |Authentication|not planned

This is the current state of implemented Advertising Data:

Advertising Data|Format|Status
----------------|------|------
Service UUID|Incomplete List of 16-bit Service UUIDs|implemented
 |Complete List of 16-bit Service UUIDs|implemented
 |Incomplete List of 32-bit Service UUIDs|not planned
 |Complete List of 32-bit Service UUIDs|not planned
 |Incomplete List of 128-bit Service UUIDs|implemented
 |Complete List of 128-bit Service UUIDs|implemented
Local Name|Shortened Local Name|implemented
 |Complete Local Name|implemented
Flags|Flags|implemented
Manufacturer Specific Data|Manufacturer Specific Data|planned
TX Power Level|TX Power Level|planned
Secure Simple Pairing Out of Band||not planned
Security Manager Out of Band||not planned
Security Manager TK Value||not planned
Slave Connection Interval Range||not planned
Service Solicitation||not planned
Service Data||not planned
Appearance|Appearance|implemented
LE Role|LE Role|planned

This is the current state of the Link Layer implementation:

Aspect | Feature | Status
-------|---------|--------
Roles|Slave Role|implemented
 |Master Role|not planned
Advertising|connectable undirected advertising|implemented
 |connectable directed advertising|implemented
 |non-connectable undirected advertising|planned
 |scannable undirected advertising|planned
Device Filtering||implemented
Connections|Single Connection|implemented
 |Multiple Connection|not planned
Connection|Slave Latency|planned
Feature Support|LE Encryption|not planned
 |Connection Parameters Request Procedure|planned
 |Extended Reject Indication|planned
 |Slave-initiated Features Exchange|planned
 |LE Ping|implemented
 |LE Data Packet Length Extension|planned
 |LL Privacy|not planned
 |Extended Scanner Filter Policies|not planned

Pullrequests are wellcome.

## Dependencies
- boost for Unittests
- CMake for build
- a decent C++ compiler supporting C++11
- btstack for experimental L2CAP support
