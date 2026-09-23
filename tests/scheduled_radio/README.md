# Testing a scheduled radio implementation

This directory is the test rig for implementations of the `scheduled_radio` concept of
`<bluetoe/scheduled_radio2.hpp>`: the tests, the instruments they run through, and the firmware
that puts an implementation on a board. The tests run on a host computer against real hardware.

## The setup

Three parts, two of them boards:

- The **host** runs the tests, with Boost.Test like the rest of Bluetoe's tests, and talks to the two
  boards over serial links.
- The **device under test** is a board running the radio implementation inside a *rig*
  (`instrument/dut_rig.hpp`). The rig takes the place of a link layer: it receives the radio's
  callbacks, runs the program a test loaded, records what happened, and answers the host.
- The **tester** is a second board that observes the air and drives it (`tester/`): it timestamps
  every PDU it hears, answers a PDU one inter frame space after it ended, and plays the central of a
  connection event.

Both boards hang off the host's USB, whose probes are also their serial ports; one wire runs from
the tester to the reset input of the device under test; a coaxial cable between the two antenna
connectors keeps the air out, if the boards have them. The READMEs of `dut_rigs/` and `tester/` give
the details.

## Principles

**The tests run on the host; the instruments only execute programs and report.** The behaviour
that has to be real time, answering after the inter frame space, transmitting at a given time,
timestamping an arrival, is armed in advance on the boards; arranging a scenario and asserting over
what was recorded has no timing requirement, so it happens where tests are easy to write, run and
debug.

**A test is a program on each instrument.** The host loads a program into each instrument, starts
the tester and then the device, waits until both report that they finished, and then collects what
each recorded and asserts over it. A device program is steps of "on this callback, make these
calls", every time relative to the time the callback carried; a tester program is operations, each
run from the end of the previous one until it received what it waits for or its window closed.
`host/program_builders.hpp` is how a test writes both.

**Two clocks that are never related.** The device reports times in its own microseconds, the
tester in ticks of its own 16 MHz timer, and nothing relates the two. A timing test has the device
transmit twice and compares the interval as the device requested it with the interval as the
tester observed it: the same two events give the same interval in both domains, up to the
implementation's placement error and the drift of its clock.

**Every time comes from the radio.** The interface has no function that returns the current time,
and neither instrument offers one to the host: every `abs_time` is the time of something that
happened on the radio, carried by the callback that reports it, and every time a program asks for
is relative to one of those. The rig has no clock of its own for the same reason: with one, a test
could establish time without ever exercising the times under test.

**The device never waits on the host.** The link is half duplex: the host asks, and gets exactly
one response per request, however long the call takes; callbacks are queued and collected
afterwards. The device's main loop is "answer a complete request if one is buffered, then call
`run()`", with no blocking read that could hold it while a radio event needs servicing.

**Loss is detectable.** Both instruments count what they record, whether they could keep it or
not, and every batch handed to the host names the index of its first entry and that count; an entry
that was dropped shows as a gap and voids the test, instead of turning "the radio did not call me"
into a passing assertion.

**A session token in every response.** The host gives an instrument a random token, which every
response echoes; the variable holding it is zeroed at startup like any other, so a restart, however
caused, reads back as zero. That catches a device that reset in the middle of a test, and it proves
the reset line: set a token, reset, require zero.

**A hardware reset before every test.** The tester drives the reset input of the device under test,
so that each test starts from a known state, including whatever the radio does before it reports
itself ready, and an unattended run recovers from a device that hung.

**Each instrument answers only the other.** The tester hears the advertisers around it, and the
device's receive window can catch one, so each instrument matches the sender's address against an
acceptance filter loaded with the other's address by the fixture: the device filtering of the Core
Specification, Vol 6, Part B, 4.3, which is also how a link layer uses the radio. A stranger's
packet can still cost a PDU of the device now and then, which is why a failed radio test is repeated.

**The tester keeps the inter frame space, and nothing else.** It holds no protocol state: a test
builds the PDUs of a connection event on the host, with the sequence numbers and flags of the flow
it expects (`host/central.hpp`), and the tester sends them as told. A device that deviates shows in
the replies the tester captured, where a tester that followed the device would hide it.

**The rig is the same on every platform; the port is not.** An instrument is the radio, the rig,
and a serial port. The port is constructed on two ring buffers the rig owns, moves bytes between
them and the hardware from below the radio's priority, and holds the host off by flow control, so
that nothing is lost. The rig therefore compiles on the host against dummies and is tested there
before anything is flashed, since a bug in the rig would present as a bug in the radio.

**The interfaces are concepts.** `scheduled_radio< Radio, CallBacks >` and `serial_port` are C++20
concepts, checked where a rig is instantiated and tested against models in
`self_tests/concept_tests.cpp`.

## Layout

| directory | contents |
|---|---|
| `link/` | what every party shares: the frames, the serialiser, the ring buffers, the function list the wire is keyed on, the program types of both instruments, the port concept |
| `instrument/` | the two rigs and what they share, the dispatcher, the reported queue, the acceptance filter set; hardware independent, runs on a board |
| `host/` | the host's side: the proxy and the transports, the program builders, the central model, and the dummies the host instantiates the rigs with to obtain their function lists |
| `test_tools/` | what the radio tests are written with: the connections to the instruments, the fixture, the environment variables, the timeline and record matchers |
| `self_tests/` | the instruments testing themselves, on the host, run by `ctest`; see its README |
| `radio_tests/` | the tests of a radio implementation, against a device, run on demand with `ctest -L radio`; see its README |
| `dut_rigs/` | the firmware of the devices under test, one per platform and radio configuration; see its README |
| `tester/` | the tester's firmware, one board; see its README |
| `nrf52/` | the UART port the nRF52 rigs and the tester share |

## Testing a radio of your own

A device under test is a firmware around your implementation: copy `dut_rigs/template_dut_rig.cpp`,
fill in the radio and the names, and give the platform a port that satisfies `serial_port`
(`link/serial_port.hpp`; `nrf52/uart.cpp` is one). The concepts are checked where the rig is
instantiated, so a missing requirement is a compile error that names it. The board's reset input
has to act as one; on the nRF52 that is `PSELRESET` in the UICR, which `platforms/` programs. Then:

1. `ctest -LE radio`: the self tests, which need no hardware, prove the rig and the link.
2. `BLUETOE_DUT=<port> ctest -L radio`: the tests that need only the device, today the pairing
   toolbox.
3. With the tester wired to the device (`tester/README.md`):
   `BLUETOE_DUT=<port> BLUETOE_TESTER=<port> ctest -L radio --repeat until-pass:3`, everything.

A test that needs what the radio may lack is skipped and reported as such, not failed: the pairing
toolbox and the 2 Mbit PHY are read from the radio's properties (`dut_supports`), the tester from
the environment (`tester_present`). `radio_tests/README.md` describes the environment variables.

## What is covered, and what is not

The radio tests cover advertising events, their timing, channels, payload sizes and access address;
scan requests and the response at the inter frame space, with the acceptance filter; connection
events with the tester as the central, with PDUs up to the largest payload, the more data bits, the
inter frame space at its edges, the receive window, CRC errors, a full receive buffer and two
connections on one radio; `set_phy()` with the 2 Mbit PHY; `cancel_radio_event()` and the timer;
the reset line; and the pairing toolbox, against the vectors of the Core Specification; and
the encryption of connection events, with the host computing the CCM of the Core Specification
(`test_tools/encryption.hpp`, checked against its sample data): an exchange in both directions, the
switches turned one at a time, a MIC that does not check, the packet counters across repeated and
empty PDUs, and 2 Mbit.

Not covered:

- The tester's own accuracy. Its receive timestamps are calibrated against the inter frame space the
  device's hardware keeps, so an inter frame space it measures on that device is right by
  construction; it has not been checked against an independent reference, and it runs on the
  board's stock crystal, where a temperature compensated oscillator is intended.
- The timer, `schedule_timer()`, is checked for consistency, not measured: nothing it does is
  visible on air, so a test can check that the callback comes with the time that was requested,
  not that either is true.
- Tolerances. The drift over an interval follows from `sleep_time_accuracy_ppm`; the placement
  tolerance of an implementation is what the tests assume, not yet what the interface promises.
- Two scan request cases the tester cannot produce: a second request within one advertising event,
  and a request with a CRC error.
- The coded PHY, and a PHY that differs between the two directions; the nRF52 radio implements
  `set_phy()` symmetric only.

## The link layer on this interface

The link layer is built on this interface: it owns the PDU buffer and hands it to the radio, the
way the device rig does, and `device.hpp` names the radio of `nrf52_radio.hpp`. The old radio
binding and its contract header are gone; the header still carries its `2` in the name.
