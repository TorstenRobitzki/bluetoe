# Candidate Tests for a Scheduled Radio Implementation

These are sketches, written to find out whether the three interfaces are enough to express the
tests we want. Each one names the calls it needs. Where a test cannot be written with the
interfaces as they stand, that is recorded at the end rather than papered over.

The host loads a program into each instrument, starts them, and asserts over what was recorded
afterwards. "DUT" is the device under test, "tester" the observing instrument. The first two
tests are written out in first_tests.cpp.

## Two time domains that are never related

The instruments count time independently and nothing in the setup relates the two domains. A
timed test does not need that: it has the DUT transmit twice, reads the two requested times
from the DUT's executed calls and the two observed times from the tester, and compares the
intervals. The same two events give the same interval in both domains, up to the placement
error of the implementation and the drift of its clock over the interval.

## Timing of scheduled events

**An advertising event is transmitted at the requested time.** The DUT program calls
`start_advertising()` on start, then schedules two timed events, each an interval after the `adv_timeout`
that ended the previous. The tester listens on the channel. Expect three PDUs, and the interval
between the second and the third as observed to match the interval as requested, within the
implementation's placement tolerance plus the drift over the interval. The first event has no
requested time; it is the origin.

**An advertising event goes to the requested channel only.** The DUT program transmits one PDU
on channel 37 and then a different one on the channel the tester listens on. Expect exactly the
second PDU. Listening on a channel that should stay silent, and only that, would also pass with
a tester that hears nothing at all.

**A scheduled event that is already in the past is refused.** The DUT program schedules an
advertising event with a delta of zero, which is the callback's own time and therefore already
gone by when the call is made; expect the recorded result to be `false` and the tester to
observe nothing. Where exactly the boundary lies is not measured, see the next test.

**Connection events at the shortest interval.** The DUT program starts with an advertising
event as the origin, schedules a connection event after it, and on every `connection_timeout`
schedules the next one 7.5 ms after the start of the previous, a few hundred times. Expect every
recorded result to be `true` and the recorded start times to be 7.5 ms apart. This is what shows
that an implementation accepts requests close enough to the callback for a link layer to hold
the shortest connection interval the specification allows, which is the only thing that has to
be known about how close is close enough. Once the tester can transmit relative to a PDU it
received, the same program with the tester answering in every window turns the consistency check
on the DUT's own times into a measurement on air.

**Two events cannot be scheduled at once.** A step that schedules an advertising event and then
another one in the same step; expect the second recorded result to be `false`.

## Receiving

**A response within the window is reported.** The tester program answers the next PDU it
receives after one inter frame space; the DUT program schedules an advertising event. Expect an
`adv_received` callback carrying the bytes the tester sent.

**No response produces a timeout.** The same, with the tester only listening. Expect
`adv_timeout` and no `adv_received`.

**The edges of the receive window.** Repeat the response test with the tester's delay swept
from shorter than the inter frame space to considerably longer, and record the range over which
`adv_received` still occurs. This is the measurement the whole setup exists for, and it is the
reason the delay is a parameter of `respond_to_next` rather than fixed.

## Connection events

**A connection event receives and transmits.** Both programs set the access address and CRC
init; the DUT schedules a connection event, the tester transmits a data PDU inside the window.
Expect the tester to observe the DUT's response and the DUT to report a `connection_end_event`.
Where inside the window the tester transmits has to be counted from something the tester saw,
so the DUT program transmits an advertising PDU first and the tester program places its data
PDU relative to the moment it received that. That is an operation the tester does not have yet.

**A cancelled connection event does not go on air.** Schedule an event far enough ahead and
cancel it in the same step. Expect the recorded result of the cancel to be `true`, no
`connection_end_event` or `connection_timeout` afterwards, and nothing observed by the tester.
The cancel's answer is definitive, so the absence of the callbacks is part of what is asserted.

## Housekeeping

**`radio_ready` is reported once.** After a reset, exactly one; on any later collection, none.

**Callback loss is detectable.** Provoke more callbacks than the queue holds without collecting,
then collect and expect either a gap in the sequence numbers or a non-zero `lost_events`.

## Optional features and dependent tests

Parts of the interface are optional, such as the 2 Mbit PHY, encryption and the synchronised
user timer, and `properties()` reports which of them an implementation has. A test of an
optional feature must not run its body conditionally, because a body that never ran is reported
as a pass. Boost.Test has the right tool: the `precondition` decorator skips a test at run time
when a predicate returns false, and the run reports it as skipped, separately from passed and
failed. The predicate asks the DUT:

```cpp
BOOST_FIXTURE_TEST_CASE( advertising_on_the_2mbit_phy, rig_fixture,
    *boost::unit_test::precondition( dut_supports{ &radio_properties::hardware_supports_2mbit } ) )
```

Decorating a `BOOST_AUTO_TEST_SUITE` the same way skips every test in it, which is the natural
shape for the tests of one optional feature.

The `depends_on( "name" )` decorator skips a test when the named one failed. Almost every timing
test presupposes that the DUT reaches the air at all; with the dependency stated, a DUT that
never transmits produces one failure and a list of skipped tests, instead of one failure per
test, each after its own host timeout, hiding the cause among its consequences.

The predicates run before the fixture of a test is constructed, so the links to the instruments
cannot be opened by `rig_fixture`. They are opened once per run by a global fixture
(`BOOST_TEST_GLOBAL_FIXTURE`), which also checks the protocol versions once; `rig_fixture` only
does the per-test reset.

## What writing these tests revealed

**The connection event tests cannot be written yet.** In the interface the implementation asks
its callbacks for a PDU buffer, and the rig interface says nothing about how the host puts a PDU
into that buffer or reads what was received into it. Scheduling a connection event is
expressible; saying what should be transmitted in it is not. The rig needs a way to set the
outgoing PDUs and to read the incoming ones, and it should be described in the same terms as
the buffer the implementation is handed, not invented separately.

**The timer cannot be measured, only checked for consistency.** `schedule_timer` produces
nothing the tester can observe, so the only witness to when the callback happened is the DUT's
own clock, which is one of the things under test. A test can check that the callback arrives and
that the time it carries matches the time requested, but not that either is true. Measuring it
would need the timer to cause something on air, which the interface deliberately does not offer.
Worth deciding whether that gap matters, given that the timer exists to schedule user callbacks
against connection events.

**Tolerances have nowhere to come from.** Several tests above say "at the requested time" without
saying how close that has to be. Until each function states its observable effect and the
tolerance it promises, every one of these tests has a number in it that someone chose while
writing the test rather than while designing the interface. The first test shows what can be
derived and what cannot: the drift over an interval follows from `sleep_time_accuracy_ppm`,
which the interface already has, and the placement tolerance is what the implementation still
has to state.

**The tester can only count from a PDU it received.** Every test that only observes gets by
without any origin on the tester. A test in which the tester has to hit a window the DUT opened
cannot, because the window is expressed in the DUT's domain and the tester has no clock origin
of its own that a test could use. The tester needs an operation whose time is relative to a PDU
it received, so that the DUT can mark the origin on air and the tester can count from there.
