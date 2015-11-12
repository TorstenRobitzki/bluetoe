/*! @mainpage bluetoe

@section intro_sec Introduction

Bluetoe is an attempt to simplify the implementation of firmware for Bluetooth Low Energy devices. Bluetooth Low Energy devices / peripherals usally implement a so called GATT Server. GATT is a protocol that alows a computer (desktop, phone etc.) to discover a devices capabilities and to interact with a device in a unified manner.

A lot of possible device capabilities are specified by the Bluetooth Special Interest Group. Others capabilities are user defined and make sense only to the implemeter of a device and the used client. Those capabilities and the means how to access them are called profiles.

<add links to bluetooth.org>

@section gatt_basics GATT Basics

@subsection Characteristics

The basic building blocks of GATT are characteristics. A characteristic can be thinked of as a piece of information / a variable that resides inside of a device, which clients can interact with. To identify a characteristic, an identifier, called a UUID is used. Beside some very basic properties like "readable" or "writeable", a characteristic can have additional properties like a name or structure informations (e.g. this is a structure containing 1 float followed by 2 integers).

@subsection Services

@subsection Profiles

@subsection UUIDs

A set of characteristics that are usually implemented together to

@section start_bluetoe Bluetoes Implementation of GATT

@section define_gap And what's with GAP?

GAP is an other important protocol that allows a GATT client to discover devices, connect to them and to gather basic informations about a device. In Bluetooth all possible options related to GAP are passed as options to the server type definition.

@section design_goals Design Goals
*/