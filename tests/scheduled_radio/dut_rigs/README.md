# The devices under test

A device under test is a firmware around a scheduled radio implementation: the rig of
`instrument/dut_rig.hpp`, the platform's serial port, and the radio. There is one per platform
and radio configuration; `../README.md` describes the setup they are part of.

This is a firmware project on `platforms/`, like `examples/`; what it needs, the toolchain, the
Nordic SDK headers and the board variables, is described in `platforms/README.md`.

## Building and flashing

For the nRF52840-DK, PCA10056, with the J-Link's serial number for flashing:

```bash
cmake -S tests/scheduled_radio/dut_rigs -B build_dut_rigs -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBLUETOE_BOARD=PCA10056 -DBLUETOE_JLINK=<serial number> -DNRF5_SDK_ROOT=/path/to/nrf5_sdk
```

```bash
ninja -C build_dut_rigs nrf52_dut.flash
```

There is one rig per sleep clock of the radio (`bluetoe/nrf.hpp`), since the placement of an
event across the two clocks and the switching of the high frequency crystal are what the tests
have to see: `nrf52_dut` on the sleep clock synthesized from the crystal, the default, `nrf52_dut_lfxo` on
the 32.768 kHz crystal, and `nrf52_dut_lfrc` on the calibrated RC oscillator. A full run flashes
and tests each in turn; the radio's accuracy the tests derive their tolerances from comes from
the rig's properties.

The rig reports the source state it was built from, `git describe --always --dirty` at configure
time, so a test log names the firmware it talked to. Reconfigure after a commit to update it.

## Running the radio tests against it

The development kit's UART is routed to the J-Link's virtual COM port, so the probe that flashed
the rig is also the host's serial device:

```bash
BLUETOE_DUT=/dev/tty.usbmodem<serial number>1 ctest --test-dir build -L radio --output-on-failure
```

See `../radio_tests/README.md`.

## Layout

| directory | contents |
|---|---|
| `nrf52/<rig>.cpp` | one file per rig, `template_dut_rig.cpp` with the radio and the names filled in |
| `../nrf52/uart.hpp`, `uart.cpp` | the serial port of the nRF52 rigs, shared with the tester: the UART in legacy mode, one byte per interrupt, hardware flow control, on the development kits' pins |
