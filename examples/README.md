# Bluetoe Examples

## Predicates / Required Tools and Libraries

Bluetoe used CMake for building and GCC as compiler. Different plattform bindings require different, additional libraries. Building for Nordic Microcontrollers requires a Nordic SDK to be installed on the build computer.

To use the build process of the Bluetoe examples to directly flash examples, using a JLink debug probe, the nRF Command Line Tools must be installed (nrfjprog).

To configure the build, some CMake cache variables are required to be set.

The build tries to find the required `arm-none-eabi-gcc` on its own. If you want to use a specific version of `arm-none-eabi-gcc`, you can set the CMake cache variable `ARM_GCC_TOOL_PATH` to point to the local installation of `arm-none-eabi-gcc` (the path, wich contains the `bin` directory).

### Nordic SDK

To build for Nordic hardware, the Nordic SDK must be installed on the build machine. `NRF5_SDK_ROOT` must then point to the installation.

## Configure Examples to run on your Hardware

Currently, there is support for following eval boards:
- PCA10056 (nRF52840 based eval board from Nordic)
- PCA10040 (nRF52832 based eval board from Nordic)

To configure the build of the examples, set the cmake cache variable `BLUETOE_BOARD` to one of the values above.

### Configure Exampels to run on custom Hardware

To run the examples on hardware that is not compatible with one of the eval boards, you have to tell Bluetoe, which microcontroller / system you want the examples build for. Currently, there is support for the following microcontrollers:
- NRF52840
- NRF52833
- NRF52832
- NRF52820
- NRF52811
- NRF52810
- NRF52805

To configure the build for one of the microcontrollers, set the cmake cache variable `BLUETOE_BINDING` to one of the values above.

Note: it is not required to set `BLUETOE_BINDING` if you want to build the examples for one of the supported eval boards, in which case, you would set `BLUETOE_BOARD` to the board identification.

### Change Pin Allocations for your Hardware

The examples are using some GPIO pins to utilizes some LEDs and / or buttons. To map the used Buttons and LEDs to your hardware, please change the layout in the file resources.hpp to fit your hardware. You will find a self-explanatory section at the end of that file.

## Support for JLink

All Nordic eval boards come with an embedded Jlink SWD debug probe, that can be used to flash the examples. To configure the build to enable directly flashing of an example, set the CMake cache variable `BLUETOE_JLINK` to the serial number of the JLink that should be used.

If `BLUETOE_JLINK` is given to the build, for every example, there will be a target that is the name of the example with a `.flash` appended. For example, to build and flash the bootloader example, the target name will be: `bootloader.flash`

On a Nordic eval board, you can read the required serial number of the JLink from the white sticker on the board (the longer number on the bottom of the sticker).

## Example Configuration of CMake

In all cases, the build starts by creating a build folder in the examples directory of Bluetoe and changing into it:

    mkdir build
    cd build

Assumed, you want to build the examples for the PCA10040 eval board, you would set the `BLUETOE_BOARD` cache variable accordingly and build the examples:

    cmake -DBLUETOE_BOARD=PCA10040 ..

If you also want to configure the embedded debug probe of the eval board:

    cmake -DBLUETOE_BOARD=PCA10040 -DBLUETOE_JLINK=683004602 ..

To build all exmples:

    make all

To build and flash just the blinky example:

    make blinky.flash

If you want to build a specific examples, you can use the example name and append a `.artifacts` to the name to build all binaries (elf file, hex file and bin file) and to get the information of the size of the resulting binary. So, if you want to build, for example, just the blinky example, build the `blinky.artifacts` target:

    make blinky.artifacts

