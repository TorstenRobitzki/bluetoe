# Candidate Tests for a Scheduled Radio Implementation

These are sketches, written to find out whether the three interfaces are enough to express the
tests we want. Each one names the calls it needs. Where a test cannot be written with the
interfaces as they stand, that is recorded at the end rather than papered over.

The host drives both instruments. "DUT" is the device under test, "tester" the observing
instrument.

## Relating the two time domains

Every test that measures time starts by relating the clocks, because the two instruments count
independently.

1. Reset the DUT through the tester, and wait for its boot counter to change.
2. Collect its callbacks and expect exactly one `radio_ready`.
3. Arm the tester to receive on channel 37 for the next 50 ms.
4. Read the DUT's `time_now()`, then schedule an advertising event 10 ms later.
5. Collect the tester's received PDUs and expect one, matching the advertising data.

The DUT's requested time and the tester's observed time are now known to denote the same
instant, and everything afterwards can be expressed in either domain. The synchronisation is
also the first measurement, since it shows the advertising data reaching the air at all.

## Timing of scheduled events

**An advertising event is transmitted at the requested time.** Having related the domains,
schedule a second event 20 ms after the first and check the tester sees the two transmissions
20 ms apart. Comparing an interval rather than an instant keeps the check independent of how
accurately the domains were related.

**A scheduled event that is already in the past is refused.** Schedule an advertising event at
`time_now()` minus a millisecond and expect `false`, with the tester observing nothing. The
interesting part is the boundary: how late may a request be and still be accepted.

**Two events cannot be scheduled at once.** Schedule an advertising event, then schedule
another before the first has reported, and expect the second to be refused.

## Receiving

**A response within the window is reported.** Arm the tester to answer the next PDU it receives
after one inter frame space, then have the DUT schedule an advertising event. Expect an
`adv_received` callback carrying the bytes the tester sent.

**No response produces a timeout.** The same, with the tester only listening. Expect
`adv_timeout` and no `adv_received`.

**The edges of the receive window.** Repeat the response test with the tester's delay swept
from shorter than the inter frame space to considerably longer, and record the range over which
`adv_received` still occurs. This is the measurement the whole setup exists for, and it is the
reason the delay is a parameter of `respond_to_next` rather than fixed.

## Connection events

**A connection event receives and transmits.** Set the access address and CRC init on both
sides, have the DUT schedule a connection event, and have the tester transmit a data PDU inside
the window. Expect the tester to observe the DUT's response and the DUT to report a
`connection_end_event`.

**A cancelled connection event does not go on air.** Schedule an event far enough ahead, cancel
it, and expect a `connection_event_canceled` callback and nothing observed by the tester.

## Housekeeping

**`radio_ready` is reported once.** After a reset, exactly one; on any later collection, none.

**Callback loss is detectable.** Provoke more callbacks than the queue holds without collecting,
then collect and expect either a gap in the sequence numbers or a non-zero `lost_events`.

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
writing the test rather than while designing the interface.
