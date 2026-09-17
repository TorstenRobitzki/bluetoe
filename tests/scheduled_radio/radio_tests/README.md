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

A test that needs the tester can still lose a PDU to other advertisers now and then (see below),
about one run of `radio_advertising_tests` in 20. Let ctest repeat a failed executable, so that a
lost PDU does not fail the run; a real fault fails every attempt:

```bash
BLUETOE_DUT=/dev/tty.usbmodem1234 BLUETOE_TESTER=/dev/tty.usbmodem5678 ctest --test-dir build -L radio --output-on-failure --repeat until-pass:3
```

`BLUETOE_TESTER_MIN_RSSI` is the weakest signal the tester keeps, in dBm, for example `-40`. The
acceptance filter already keeps the tester to the device by address, so this is an orthogonal option
for a setup where address alone does not separate them: with the two boards coupled by a coaxial
cable the device arrives tens of dB above the air leaking in, so a limit between the two drops the
air, and the reported RSSI of a capture shows where the limit belongs. Left unset, the tester keeps
every PDU.

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
| `observations.hpp` | what the timing tests build their PDUs from and read their results with: advertising channel PDUs, the records and captures of a run |
| `advertising_tests.cpp` | start_advertising() and schedule_advertising_event() over the air: payload sizes, channels, intervals, the access address, and scan requests with the acceptance filter and T_IFS; skipped without a tester |
| `connection_tests.cpp` | schedule_connection_event() over the air, with the tester as the central; skipped without a tester |
| `cancel_tests.cpp` | cancel_radio_event() on advertising events: in time, too late and with nothing pending; skipped without a tester |
| `timer_tests.cpp` | schedule_timer() and cancel_timer(), made visible by an advertising scheduled from user_timer(); skipped without a tester |

A test that needs a feature the device may lack is decorated with a `precondition` on
`dut_supports`, one that needs the tester with a `precondition` on `tester_present`, so that it is
skipped and reported as such rather than failed.

## Other advertisers

Even with the two boards coupled by a cable, the tester hears the advertisers around it, about two
per test. An acceptance filter on each side keeps them off the record: the device answers only the
tester, so a stray in its window does not stall a run, and the tester reports only the device, so
what it hands back is the device's. That is the device filtering of the Core Specification,
Vol 6 Part B 4.3, set up by the fixture; see `documentation/scheduled_radio_test_rig.md`, decision 25.
The tester's radio abandons a stranger's packet as soon as its address is received, but while it
still receives one, a PDU of the device that starts at the same time is lost, and a test that expects
it fails. That is what the repeat above is for.
