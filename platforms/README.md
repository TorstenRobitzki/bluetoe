# Platform support for Bluetoe firmware

Bluetoe itself needs no platform support beyond the platform's SDK headers. No startup or toolchain
files are required by the library; the project that consumes it as a library provides them.

Bluetoe ships two projects that have to build runnable firmware binaries: the examples, which
consume Bluetoe as a library, and the test rigs of the scheduled radio, the abstraction of the radio
hardware that a link layer can be built on. This directory is what they share.

It is everything a firmware is built on: the cross toolchain, the per-binding startup code, linker
scripts and flash commands, the C++ runtime that replaces newlib's, `assert()`, and a container with
the toolchain and the SDK.

## Required tools and libraries

Bluetoe uses CMake for building and GCC as compiler. Different platform bindings require different,
additional libraries. Building for Nordic microcontrollers requires a Nordic SDK to be installed on
the build computer.

To flash a firmware directly from the build, using a J-Link debug probe, the nRF Command Line Tools
must be installed (`nrfjprog`).

The build tries to find the required `arm-none-eabi-gcc` on its own. If you want to use a specific
version of `arm-none-eabi-gcc`, set the CMake cache variable `ARM_GCC_TOOL_PATH` to point to the local
installation of `arm-none-eabi-gcc` (the path which contains the `bin` directory).

### Nordic SDK

To build for Nordic hardware, the Nordic SDK must be installed on the build machine. `NRF5_SDK_ROOT`
must then point to the installation. Only the headers under `modules/nrfx/mdk` and
`components/toolchain/cmsis/include` are used; the CI job assembles them from the public nrfx and
CMSIS repositories.

## Naming the hardware

To configure a build, set the cache variable `BLUETOE_BOARD` to one of the supported evaluation
boards:
- PCA10056 (nRF52840 based eval board from Nordic)
- PCA10040 (nRF52832 based eval board from Nordic)

For hardware that is not one of these boards, set `BLUETOE_BINDING` to the microcontroller instead:
NRF52840, NRF52833, NRF52832, NRF52820, NRF52811, NRF52810 or NRF52805. Exactly one of the two
variables is set.

## Support for J-Link

All Nordic eval boards come with an embedded J-Link SWD debug probe that can be used to flash a
firmware. Set the CMake cache variable `BLUETOE_JLINK` to the serial number of the J-Link that should
be used; on a Nordic eval board it is the longer number on the white sticker. Every firmware target
then has a `.flash` target, for example `blinky.flash`.

## How a firmware project uses this directory

A firmware project's `CMakeLists.txt` includes `cmake/firmware_project.cmake` before `project()`,
which selects the toolchain and checks the hardware variables, and `cmake/firmware.cmake` after it,
which applies the options every firmware is built with, adds this directory and the library, and
defines `add_bluetoe_firmware( target sources... )`. `examples/CMakeLists.txt` is the smallest
example of such a project.

## The build container

`docker/` holds a container image with the toolchain and the SDK headers, and a Makefile that builds
the examples inside it: `make -C platforms/docker`.
