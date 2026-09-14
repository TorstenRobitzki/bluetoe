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

The tester, once it is wired to the device (see `../tester/README.md`), is named the same way:

```bash
BLUETOE_DUT=/dev/tty.usbmodem1234 BLUETOE_TESTER=/dev/tty.usbmodem5678 ctest --test-dir build -L radio --output-on-failure
```

Without `BLUETOE_TESTER` the tests that need the tester are skipped and reported as such; with it,
every run begins with a reset of the device through the tester.

`BLUETOE_TESTER_MIN_RSSI` is the weakest signal the tester keeps, in dBm, for example `-40`. With the
two boards coupled by a coaxial cable the device arrives tens of dB above the air leaking in, so a
limit between the two drops the air and the tester hears only the device; the reported RSSI of a
capture shows where the limit belongs. Left unset, the tester keeps every PDU.

`BLUETOE_DUT_TIMEOUT_MS` bounds one request to either instrument, by default 2000 ms; a toolbox call
is answered from inside the device's dispatcher, so it has to cover a point multiplication on the
device.

Without a device, run everything else with `ctest -LE radio`, as the CI does.

## What each file covers

| file | subject |
|---|---|
| `dut.hpp`, `dut.cpp` | the connection every test uses: opened once per run by a global fixture, protocol version checked, a fresh session token set, the properties read; the restart of the device through the tester; `dut_fixture` and the `dut_supports` predicate for `precondition` |
| `tester.hpp`, `tester.cpp` | the connection to the tester, opened by the same fixture when `BLUETOE_TESTER` is set; `the_tester()` and the `tester_present` predicate |
| `environment.hpp`, `environment.cpp` | the environment variables the tests are configured by, one function each |
| `toolbox_tests.cpp` | the pairing toolbox with the Core Specification's vectors, and a key agreement with the software toolbox of the security manager tests; skipped on a device without a toolbox |
| `reset_tests.cpp` | the reset line: the device answers with a zero session token after the tester pulled it; skipped without a tester |
| `rig_fixture.hpp` | the fixture the timing tests share: resets the device, loads a program into each instrument, runs them, and hands over what each recorded; the program builders |
| `first_tests.cpp` | the first timing tests over the air: an advertising event is transmitted at the requested interval, and on the requested channel; skipped without a tester |

A test that needs a feature the device may lack is decorated with a `precondition` on
`dut_supports`, one that needs the tester with a `precondition` on `tester_present`, so that it is
skipped and reported as such rather than failed.

## Over the air, for now

The timing tests run with both boards on their antennas, so the tester hears every advertiser on
the channel and the device's advertising window occasionally catches one. The tests tell the
device's PDUs from the air's by content and tolerate a missed one, but a stray reception on the
device stalls that run, so a run may fail and is simply repeated. Ruling that out, and being able to
assert that a PDU did *not* appear, needs the two boards coupled by cable with their antennas
switched out; that is a bench change, not a code change.
