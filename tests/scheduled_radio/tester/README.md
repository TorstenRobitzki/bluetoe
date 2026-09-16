# The tester

The tester is one firmware for one board: the rig of `instrument/tester_rig.hpp`, the UART, and
what else the tester needs from the board, today the reset line to the device under test.
`documentation/scheduled_radio_test_rig.md`, decisions 4 and 23, say what it is for and why it is
laid out like this; its radio follows with decision 11, step 3.

It runs on an nRF52840-DK, PCA10056, next to the device under test. This is a firmware project on
`platforms/` for the toolchain, the startup code and the flash target; the board is preset, so only
the J-Link's serial number and the SDK are needed to configure it.

## Wiring

Two wires between the two development kits: the tester's P0.03 to P0.18 of the device under
test's kit, which is the nRF52840's reset pin and the net its RESET button and probe use, and
ground to ground. Both kits hang off the same host's USB, which joins the grounds already, so the
reset works without the second wire, but a ground wire beside the signal gives the reset edge a
clean return path. The reset line is driven open drain, so it can share the net with the kit's own
reset circuitry.

## Building and flashing

```bash
cmake -S tests/scheduled_radio/tester -B build_tester -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBLUETOE_JLINK=<serial number> -DNRF5_SDK_ROOT=/path/to/nrf5_sdk
```

```bash
ninja -C build_tester tester.flash
```

The tester reports the source state it was built from, like a device under test; reconfigure
after a commit to update it.

## Running the radio tests with it

The tester is named to the tests by a second environment variable next to `BLUETOE_DUT`:

```bash
BLUETOE_DUT=/dev/tty.usbmodem<serial number>1 BLUETOE_TESTER=/dev/tty.usbmodem<serial number>1 ctest --test-dir build -L radio --output-on-failure
```

Tests that need the tester are skipped without it. See `../radio_tests/README.md`.

## Layout

| file | contents |
|---|---|
| `tester.cpp` | the tester's rig on its board, and `main()` |
| `platform.hpp`, `platform.cpp` | the reset line and the idling, on the nRF52 development kits |
| `../nrf52/uart.hpp`, `uart.cpp` | the serial port, shared with the devices under test |
