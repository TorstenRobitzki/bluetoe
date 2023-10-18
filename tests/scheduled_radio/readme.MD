# Testing a Scheduled Radio Implementation

## Basic Idea

The scheduled_radio defines a set of functions that directly or indirectly have side effecta that are visible over the air and in the time domain.

It is essential, that all functions of a scheduled_radio are none-blocking. The basic idea is, that a scheduled_radio implementation is deployed to some hardware. This deployed implementation is named a device under test (DUT). A tester is then connected to the DUT via an interface like an UART. The tester is able to call functions on the scheduled_radio implementation via RPC (remote procedure calls) and to receive calls from the scheduled_radio implementation to the callbacks.

## Tester

The tester is a nRF52 eval board, that will have it's crystal oscillator replaced by a clock source with very high accuracy (e.g. SIT5358AN-FR-33N0-32.000000). The tester will call function on the scheduled_radio implementation and will observe activity over the air and / or interact with the DUT over the air. In addition, the tester will observe calls to the callbacks of the scheduled_radio implementation.

## Device Under Test (DUT)

The device under test combines the implementation of the scheduled_radio and the interface to connect to the tester.

## Protocol (RCP)

The test comes with its own, light weight RPC implementation.

## Test Framework

The tests themself have to be expressed somehow. Usually a test framework would provide functionality to collect all test that are distributed over several source code files and some mean to express the expected outcomes of a test.

* Boost.test: seems to be not very configurable. Have not managed to get a binary that is smaller than 1MByte.

* lest: requires exceptions to be enabled

* doctest: Fine so far...; ~625k for a single test; Mostly IO streams

* Catch2: Fine so far...; ~750k for a single test; Mostly IO streams

* Junity:

* CUTE:

## External Flash

As the test framework is pretty large, the firmware makes use of the PCA10056 external QSPI flash memory (8MByte).

## Building the tester:

Create a build directory, change into that build directory and use CMake to create a build environment:

    cmake -DARM_GCC_TOOL_PATH=/usr/local/arm-gnu-toolchain-12.2.rel1 -S ../tests/scheduled_radio -B . -DCMAKE_TOOLCHAIN_FILE=../examples/cmake/gcc-arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug -DTESTER_JLINK=1050227768 -DNRF5_SDK_ROOT=~/CMSIS/nRF5_SDK_17.1.0_ddde560  ../tests/scheduled_radio

* -DARM_GCC_TOOL_PATH= has to point to the installation directory of GCC
* -S has to point to the directory, containing this readme.MD file
* -DCMAKE_TOOLCHAIN_FILE= has to point to the gcc-arm-none-eabi.cmake in the expamples cmake directory.
* -DTESTER_JLINK= can be set to the serial number of your JLink
* -DNRF5_SDK_ROOT= has to point to an installation of the Nordic SDK

Use

    make tester.flash

to build and flash the tester onto an PCA10056 eval board.