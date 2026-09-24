# Soak tests

A soak test keeps one connection between the device under test and the tester for minutes to
days: the tester as the central sends an empty PDU every 100 ms, the device answers, and between
the events the device's user timer runs, so that the clocks switch as they do under a link layer.
Ten minutes, the default, crosses the overflow of a 24 bit RTC at 32.768 kHz and, on an RC sleep
clock calibrated every four to eight seconds, some hundred calibrations; 71 minutes cross the wrap of the
device's microsecond time; a day takes the temperature round once.

The setup is that of the radio tests (`../radio_tests/README.md`): `BLUETOE_DUT` names the device,
`BLUETOE_TESTER` the tester, and the test is skipped without one. `BLUETOE_SOAK_SECONDS` is the
duration, 600 if not set.

```bash
BLUETOE_DUT=/dev/tty.usbmodem1234 BLUETOE_TESTER=/dev/tty.usbmodem5678 ctest --test-dir build -L soak_tests --output-on-failure
```

The test has no ctest timeout, so ctest's usual 1500 s do not end it. For a run of days start the
executable itself, so that a terminal owns it and its progress is seen:

```bash
BLUETOE_DUT=/dev/tty.usbmodem1234 BLUETOE_TESTER=/dev/tty.usbmodem5678 BLUETOE_SOAK_SECONDS=172800 ./build/tests/scheduled_radio/soak_tests/soak_connection_tests
```

One line a minute reports the counts so far, and the last line the counts at the end:

```
     60 s: events 600, timeouts 0, timers 599, refused calls 0, anchor error -10..8 us, bins 598 1 0 0 0; tester events 600, replies 600, crc errors 0, unanswered 0; crystal starts 602, on 43980 ticks, calibrations 17
```

A device that timed out, a call it refused or an event the tester heard no answer to ends the run
at once, since the rest of the duration would only repeat it. A request lost on the serial link is
retried a few times before it counts as a fault, since a hiccup of the link within days is
likelier than a fault of the rig, and a fault repeats.

## What it measures

**The anchor error.** Of every connection event the device placed from the anchor of the one
before, the anchor the radio reported minus the centre of the receive window the device asked
for: the drift of the device's sleep clock over the interval, plus the placement. The test
requires the extremes within the tolerance the radio's `sleep_time_accuracy_ppm` allows for the
interval, the same tolerance the radio tests use, and reports a count per bin of magnitude, below
10, 50, 100 and 250 µs and beyond, which over days says whether the clock sits where it should or
creeps towards the edge of the window with the temperature of the day.

**The reply delay.** The time from the first bit of the tester's PDU to the first bit of the
device's reply, by the tester's clock: the inter frame space the device keeps, plus the PDU's
time on air. Required within 2 µs of the 230 µs it should be.

**The clock statistics.** A radio built with `bluetoe::nrf::clock_statistics` counts how often it
started the high frequency crystal, how many periods of the sleep clock the crystal ran in all,
and how often it calibrated the RC sleep clock; the rigs are. The test requires a start per event,
a running time bounded by the events and the calibrations, and, on an RC sleep clock, the
calibrations at the pace of the timer: at most one per four seconds, at least one per eight. A crystal left running through an interval, which
once needed a logic analyser to see, fails here. A radio on a synthesized sleep clock never stops
the crystal and counts nothing.

The test cannot measure current, but the statistics count what costs it. For the number itself,
a Power Profiler Kit on the development kit's current measurement pins logs alongside the run;
its log and the test's lines share the wall clock, so an anomaly in one is found in the other.

## How it works

The instruments count rather than record: a step of the device's program and an operation of the
tester's run as often as they say (`repeated()` in `host/program_builders.hpp`), the first run
recorded and captured as usual, the rest counted in a summary each instrument keeps since its
program started (`program_summary` in `link/program.hpp`, `tester_summary` in
`link/tester_program.hpp`). The test reads both summaries while it waits and once at the end.

The tester holds no protocol state, so its PDU is the same in every event: the same SN and NESN,
which the device's PDU buffer takes as a retransmission it has seen and as no acknowledgement of
its own empty reply. Nothing accumulates from that; the device answers every event, which is what
the test is about.
