## To build the example:
- Download the SDK from Nordic
- create a build directory (./build for example)
- change into the build directory and execute cmake to create a make file
- run make build one of the examples

## Parameters to cmake

* BLUETOE_ROOT

Path to your bluetoe copy. That path have to contain a directory named bluetoe, which counts the bluetoe sources.

* NRF51_SDK_ROOT

Path to your Nordic SDK. Bluetoe requires just very basic stuff from the SDK under /components.

* JLINK_DEVICE

Optional parameter, used if you like to use JLink to flash the examples, this parameter takes the id of your JLink. If you are using an eval board, the serial number is printed on the microcontroller that is _not_ the nrf51/52

* NRFJPROG_DEVICE

Same as JLINK_DEVICE, but this time the nrfjprog tool from Nordic is used for flashing.

### Example:

\> cmake -DNRF51_SDK_ROOT=~/CMSIS/nRF5_SDK_11/ -DNRFJPROG_DEVICE=681485618 -DBLUETOE_ROOT=~/bluetoe ..

## Build targets

For every example (blinky, bootloader, cycling_speed_and_cadence...) there are following targets:
- \<example>.elf to build an elf file
- \<example>.hex to build an hex file
- \<example>.flash to build the software and to flash it
- erase.flash erase the flash on the device

### Example:

\> make bootloader.flash
