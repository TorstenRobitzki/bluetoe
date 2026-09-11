# Bluetoe Examples

The examples are firmware for the Nordic evaluation boards. What they are built with, the toolchain,
the Nordic SDK, the board and binding variables and the J-Link support, is described in
`platforms/README.md`.

## Configure the examples to run on your hardware

In all cases, the build starts by creating a build folder in the examples directory of Bluetoe and
changing into it:

    mkdir build
    cd build

Assumed, you want to build the examples for the PCA10040 eval board, you would set the
`BLUETOE_BOARD` cache variable accordingly and build the examples:

    cmake -DBLUETOE_BOARD=PCA10040 ..

If you also want to configure the embedded debug probe of the eval board:

    cmake -DBLUETOE_BOARD=PCA10040 -DBLUETOE_JLINK=683004602 ..

To build all examples:

    make all

To build and flash just the blinky example:

    make blinky.flash

If you want to build a specific example, you can use the example name and append a `.artifacts` to
the name to build all binaries (elf file, hex file and bin file) and to get the information of the
size of the resulting binary. So, if you want to build, for example, just the blinky example, build
the `blinky.artifacts` target:

    make blinky.artifacts

## Change pin allocations for your hardware

The examples are using some GPIO pins to utilize some LEDs and / or buttons. To map the used buttons
and LEDs to your hardware, please change the layout in the file `resources.hpp` to fit your hardware.
You will find a self-explanatory section at the end of that file.
