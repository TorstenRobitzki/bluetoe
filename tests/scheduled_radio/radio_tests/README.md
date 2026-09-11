# Tests of a scheduled radio implementation

The tests in this directory are the purpose of the test rig: they test a scheduled radio
implementation through the instruments, against a real device. They need a device under test on
a serial port, and later a tester on a second one, so `ctest` does not run them by default. See
`documentation/scheduled_radio_test_rig.md`, decision 21; the tests of the instruments themselves
are in `../self_tests/`.

## Running them

The device under test is named by an environment variable, so that the same build runs against
any device and the only thing that changes is the port:

```bash
BLUETOE_DUT=/dev/tty.usbmodem1234 ctest --test-dir build -L radio --output-on-failure
```

`BLUETOE_DUT_TIMEOUT_MS` bounds one request, by default 2000 ms; a toolbox call is answered from
inside the device's dispatcher, so it has to cover a point multiplication on the device.

Without a device, run everything else with `ctest -LE radio`, as the CI does.

## What each file covers

| file | subject |
|---|---|
| `dut.hpp`, `dut.cpp` | the connection every test uses: opened once per run by a global fixture, protocol version checked, a fresh session token set, the properties read; `dut_fixture` and the `dut_supports` predicate for `precondition` |
| `toolbox_tests.cpp` | the pairing toolbox with the Core Specification's vectors, and a key agreement with the software toolbox of the security manager tests; skipped on a device without a toolbox |

A test that needs a feature the device may lack is decorated with a `precondition` on
`dut_supports`, so that it is skipped and reported as such rather than failed.
