# The devices under test

A device under test is a firmware around a scheduled radio implementation: the rig of
`instrument/dut_rig.hpp`, the platform's serial port, and the radio. There is one per platform
and radio configuration; `documentation/scheduled_radio_test_rig.md`, decision 22, says how they
are laid out and why.

This is a firmware project on `platforms/`, like `examples/`; what it needs, the toolchain, the
Nordic SDK headers and the board variables, is described in `platforms/README.md`.

## Building and flashing

For the nRF52840-DK, PCA10056, with the J-Link's serial number for flashing:

```bash
cmake -S tests/scheduled_radio/dut_rigs -B build_dut_rigs -G Ninja -DBLUETOE_BOARD=PCA10056 -DBLUETOE_JLINK=683004602 -DNRF5_SDK_ROOT=/path/to/nrf5_sdk
```

```bash
ninja -C build_dut_rigs nrf52_dut.flash
```

The rig reports the source state it was built from, `git describe --always --dirty` at configure
time, so a test log names the firmware it talked to. Reconfigure after a commit to update it.

## Running the radio tests against it

The development kit's UART is routed to the J-Link's virtual COM port, so the probe that flashed
the rig is also the host's serial device:

```bash
BLUETOE_DUT=/dev/tty.usbmodem0006830046021 ctest --test-dir build -L radio --output-on-failure
```

See `../radio_tests/README.md`.

## Layout

| directory | contents |
|---|---|
| `nrf52/uart.hpp`, `uart.cpp` | the serial port of the nRF52 rigs: the UART in legacy mode, one byte per interrupt, hardware flow control, on the development kits' pins |
| `nrf52/<rig>.cpp` | one file per rig, `instrument/template_dut_rig.cpp` with the radio and the names filled in |
