# Testing a Scheduled Radio Implementation: Design Decisions

This document records the decisions behind the test setup for a `scheduled_radio`
implementation, together with the reasoning and the alternatives that were rejected. It
complements *Bluetoe Link Layer Design Considerations* (October 2023), which motivates the move
to absolute times and describes the limitations this work addresses.

The subject is testing a *radio* implementation, not a link layer. The link layer is deliberately
absent from the setup, and several decisions below depend on that.

## 1. The tests run on a host, not on the tester

Test cases and their sequencing run on a host computer. The tester and the device under test are
instruments, driven over serial links.

The split that matters is not tester against host, it is real time against everything else. The
behaviour that must be real time is pre-armed rather than decided: respond to a received PDU after
T_IFS, transmit at a given absolute time, timestamp an arrival. Arranging a scenario, triggering it
and asserting over collected timestamps has no timing requirement at all.

Tests are then written with the Boost.Test already used throughout the project, no embedded test
framework is needed (the evaluated ones cost between 600 and 750 kilobytes and drove the external
QSPI flash and the linker script restructuring), and a failing test rebuilds in seconds instead of
being flashed.

The simulated radio in the existing host tests is not a subject of these tests. It is a fixture for
testing the link layer, and the tests described here will look very different from the ones that
use it. What the two can share is the specification: once every function of the interface states
its observable effect and its tolerance, by decision 10, any implementation can be held to that
contract, the simulated radio included. Issues #84 and #134 exist because the fixture and the nRF52
disagree and nothing notices. This setup does not fix that on its own; what it can eventually
provide is the ground truth to correct the fixture against.

**Rejected:** running the tests on the tester, which was the earlier direction. The analogy with
commercial protocol testers does not carry: what forces logic into those is a live protocol state
machine that must acknowledge, follow the channel map and answer control PDUs inside connection
events. None of that exists when the link layer is not under test. Commercial testers also run
their suites on a host; their instrument boundary simply sits lower because their instrument has to
know far more.

This decision should be revisited when the link layer itself is tested over the air, at which point
behaviour migrates into the tester. The property to preserve then is that the vocabulary of the
instrument grows, not that the tests move.

## 2. Both instruments connect to the host directly

The tester and the device under test each have their own link to the host. The links are not
chained through the tester.

The device under test is the thing expected to hang, and that is partly what is being tested. If
tester traffic were routed through it, a stuck device would take the observer offline at exactly
the moment its observations are needed. Independent links also keep forwarding logic out of the
tester, avoid head-of-line blocking between the two conversations, and allow either side to be
brought up or debugged alone.

## 3. No trigger line between the tester and the device under test

There is no shared GPIO used to establish a common time origin.

The device under test has no timebase of its own to timestamp an edge with, by decision 5, and
between radio events only the low frequency oscillator is running, so producing a high resolution
timestamp on demand is not reliably possible. A pin driven from hardware would travel the same path
as the transmitted energy and would therefore confirm nothing that was not already assumed. A pin
asserted from software would measure the device's belief about its own timing, which the interface
never promises.

Instead the air is the shared observable. The device under test schedules a transmission at a time
of its own choosing and reports that `abs_time` in its reply; the tester timestamps the arrival with
its accurate clock. That single exchange relates the two time domains, and the synchronisation is
itself a measurement of the property under test. Where drift matters, roughly five microseconds per
hundred milliseconds at the worst clock error the specification permits, a test re-synchronises by
transmitting again.

## 4. A reset line from the tester to the device under test is kept

The tester drives the reset input of the device under test.

Two reasons. Each test starts from a known state, which is not trivial for this driver because
calibration state, the high frequency oscillator and the CCM counters all persist. And the host can
recover from a hung device: without a reset line a single failing test ends an unattended run and
needs someone at the bench.

The reset must be a hardware reset rather than a software restart, because initialisation is part
of what is being tested, including whatever the radio does before it reports itself ready.

Mechanism belongs in the tester, policy on the host: the tester owns the pin and exposes a reset
command, the host decides when to use it.

## 5. The rig owns the serial link only; the radio owns the rest of the hardware

The rig on the device under test gets access to the scheduled radio implementation and a serial
link served at low interrupt priority. The scheduled radio implementation keeps exclusive access to
every other hardware resource, as it does today.

The obvious reason is that the rig must not perturb what it measures. The better reason is that
denying the rig a timebase forces every test to obtain time through `time_now()` and the timestamps
carried by the callbacks, which are exactly the contract under test. A rig with its own clock would
let tests establish time without ever exercising the interface's time functions.

## 6. Half duplex, with the host always initiating

The device under test never speaks unsolicited. A call that has a result becomes a call followed by
polling for that result. Callbacks are queued and collected by polling.

This buys the property that matters most: the device under test never waits on the host. Its main
loop becomes "answer a pending request, otherwise call `run()`", with no blocking read anywhere, so
it can never be parked in a receive while a radio event needs servicing. That failure mode would
quietly invalidate the timing results the rig exists to produce.

It also removes any question of both ends transmitting at once, lets one implementation serve both
endpoints, and keeps the rig contract portable to transports that are genuinely half duplex. Since
all timing information travels in the payload rather than in the arrival of a message, polling
costs bench time and nothing else.

## 7. The callback queue must make loss detectable

Callbacks occur while the host is not listening, so they are queued with their timestamps, and the
queue can overflow.

A silently dropped callback would corrupt a test in the most misleading way available, by turning
"the radio did not call me" into a passing negative assertion. The queue therefore carries either a
sequence number the host checks for gaps or an overflow flag that latches until read. The depth is
a deliberate parameter, since it sets how long a test may run between polls.

## 8. A boot counter in every response

Every response carries a counter that increments on each reset of the device under test.

Under decision 6 the device may not announce itself after booting. A counter checked on every
exchange is better than an announcement would have been: it cannot collide, it is checked
continuously rather than only when a reset was expected, and it detects a restart that happened in
the middle of a sequence, which is the failure mode that occurs while bringing up a new radio.

## 9. The rig contract is a separate artefact from the radio interface

The rig has its own small contract, stated independently of any particular hardware: a hardware
reset input with a bounded worst case time to ready, a serial link with defined framing, an
identity and version report, and the boot counter of decision 8.

How a given part satisfies that contract is the port's business. On the nRF52 the reset pin only
acts as one if `PSELRESET` is programmed in the UICR, which is a one-time configuration that is
easy to forget and confusing to debug because the line simply appears dead. Other hardware will
have other details.

Together with the interface specification this forms a conformance kit: a new port implements the
interface, satisfies the rig contract, and the existing test suite runs against it unchanged. The
design considerations document notes that the proof that the abstraction is implementable on other
hardware is still outstanding. This is how that proof gets produced.

## 10. Every interface function states its observable effect

Each function of the interface documents what is externally observable when it is called: what
appears on air, at what time, and within what tolerance.

This is what makes the interface testable rather than merely described, and it lets the test
definitions follow from the specification instead of being invented separately afterwards.

## 11. Order of work

1. The interface specification, including decision 10.
2. A narrow but real implementation on the nRF52: `time_now()`, scheduling an advertising event,
   and the callbacks that event produces.
3. The rig and the tests, broadened together with the implementation.
4. The link layer.
5. The additional CPU context for link layer processing.

Something implemented comes before the rig because nothing yet demonstrates that the interface is
implementable with the timing it promises, and building a rig against an unimplemented interface
risks baking in assumptions the hardware will not honour. It does not have to be the whole radio.
Absolute scheduling of one advertising event, with the time reported back from the hardware
capture, exercises the claim the whole design rests on, and it is a small part of the work.

**Not an adapter over the existing radio.** The obvious shortcut is to wrap the current
implementation, and it does not work. The new interface exists to report when things happened,
taken from the hardware capture, and the existing implementation neither exposes a clock nor
carries a time on its callbacks. An adapter could only read a clock in software after the fact,
which is exactly the error the redesign removes, so it would produce timings that look plausible
and mean nothing. Reaching into the timer hardware to do better stops being an adapter.

Writing it from scratch also measures a claim the design considerations document makes, that the
new interface should lower the complexity of a radio implementation. An adapter would say nothing
about that; a fresh implementation says it directly.

The additional CPU context is built last but designed now. Even while everything runs in one
context, the specification should name the context each callback is invoked from, name the mutual
exclusion primitive the radio must provide, and settle what replaces `run()`, which the callbacks
currently refer to but which the interface does not declare. Naming those now makes the later split
a change of implementation rather than of contract.

## 12. The comparison window of `abs_time`

`abs_time` values form a ring, and the comparison operators are only meaningful when the two values
lie within `max_distance` of each other. That precondition should be stated in the header and
asserted in debug builds, because a caller comparing a deadline against `time_now()` across a
larger gap gets a silently inverted answer.

The implemented value of 16.78 seconds is correct, for a reason the design considerations document
does not give. The core specification caps the supervision timeout at 32 seconds and requires it to
exceed twice the latency times the interval, so the largest legal gap between two connection events
a peripheral must handle is just under 16 seconds. The document's figure of 2000 seconds, from a
four second interval and a latency of 500, ignores that constraint and would suggest a far larger
window than is needed.

## 13. This work is not restricted to C++11

The rest of Bluetoe is C++11 and stays that way. Everything belonging to this work may use whatever
standard helps, and the parts that already do, such as the concepts in the remote call layer, are
not a problem to be removed.

The scope of that is everything under this banner: the new radio interface, `abs_time`, the rig,
the tester and the host side tooling. It does not extend to the existing link layer, the GATT
layer, or the current radio bindings, which keep their existing constraint until something
deliberately changes it.

Two consequences to keep in mind. `abs_time.cpp` and `radio_properties.cpp` are currently compiled
into `bluetoe_linklayer`, which the existing link layer, both Nordic bindings and most of the host
tests link against. Raising the standard on that target raises it for all of them, so these sources
belong in a target of their own before they start using anything newer. And the continuous
integration build pins the whole configuration to strict C++11 precisely to catch violations in the
existing library, so whatever target holds this work has to set its own standard rather than
inherit that pin.

## Open questions

- Whether the interface needs an equivalent of `run()`, and from which context callbacks are
  delivered. This is entangled with decision 11.
- The depth of the callback queue, and what the host does when it detects loss.
- How the tester itself is validated. Its timestamps and its T_IFS response are the measurement, so
  an error there presents as a fault in the device under test. Checking it against a known good
  device or against a sniffer is a prerequisite for the rig rather than an afterthought.
