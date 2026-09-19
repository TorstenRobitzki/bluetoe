# Candidate Tests for a Scheduled Radio Implementation

These are sketches, written to find out whether the three interfaces are enough to express the
tests we want. Each one names the calls it needs. Where a test cannot be written with the
interfaces as they stand, that is recorded at the end rather than papered over.

The host loads a program into each instrument, starts them, and asserts over what was recorded
afterwards. "DUT" is the device under test, "tester" the observing instrument. The tests of
advertising are written out in radio_tests/advertising_tests.cpp.

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

**A radio copes with the tolerance of T_IFS.** The Core Specification allows the inter frame space
to deviate by ±2 µs. Have the tester answer the DUT at 148 µs and at 152 µs, and expect the DUT
to receive the answer in both cases: a scan request answered with its scan response, and later a
data PDU in a connection event answered with the next one. This needs the tester to place an
answer to well below a microsecond, so it waits for the temperature compensated oscillator of
decision 24 and for the tester to be validated against an independent reference; until then the
tester's receive timestamps are calibrated against the DUT's own T_IFS, and a deviation of 2 µs
cannot be told from that calibration. The connection event half is written, with that caveat in its
comment; the scan request half waits for the answer's delay to become a parameter.

## Connection events

Every test starts with the DUT advertising: the tester places its first connection event from that
advertising (`connection_event`), after both switched to the connection's access address. The
tester's program is loaded before the run, so its SN, NESN and MD bits are what the test expects the
flow to be; a host side model of the central builds them, and a DUT that deviates shows in the
replies the tester captured. Encryption is left for a later batch. The entries below are written
out in radio_tests/connection_tests.cpp, except where an entry says what is missing.

Several entries look at the same behaviour from different sides, the MD flag or a CRC error as an
end of the event and as a flag of `connection_event_events`, for example. Such a behaviour gets one
test, not one per entry.

### Normal flow

**One PDU each way, both empty.** The DUT answers the tester's empty PDU with an empty one, the event
closes as neither has MD set, and `connection_end_event` carries the anchor.

**PDU sizes.** Vary the payload in both directions: empty, a middle size, 27 bytes, and up to
`radio_max_supported_payload_length`. Expect the bytes on both sides and the answer at T_IFS after
the end of a PDU of each length. Written for 1, 13 and 27 bytes. More than 27 is the data length
extension and more than the setup carries: a PDU on the wire holds 39 bytes, the tester's radio 37
of payload, the rig's buffer 27, and a PDU of 251 bytes would not fit into one request, so it would
have to be generated on the instrument rather than carried to it.

**The MD flag.** Both MD clear closes the event after one exchange; the tester's MD set keeps the
DUT listening; the DUT's MD set, with more queued, keeps the event going while the tester transmits.

**The channel changes.** Two connection events on different data channels, the tester following.

**The access address is used and can be changed.** A tester on the connection's access address gets
answers, one on another address none; after the DUT switches, the tester follows.

**2 Mbit.** Sending and receiving at 2 Mbit, on a DUT that supports it. Not written: needs 2 Mbit in
the tester and in the DUT's radio.

**The times reported are correct.** The next event is scheduled from the time `connection_end_event`
or `connection_timeout` carried, and the tester measures the distance of the anchors.

### How an event ends

One test for every end the interface names:

- nothing is received between `start` and `end`: `connection_timeout` carrying `end`;
- neither side has MD set: `connection_end_event` after the DUT's answer;
- nothing is received after the DUT's answer: `connection_end_event`;
- the second PDU in a row with an invalid CRC: `connection_end_event`, without an answer;
- the event is cancelled: nothing is reported.

### Errors

**The receive window.** A first PDU from the tester before `start` and after `end` is not received;
one just after `start` and one near `end` are. A PDU is received if its first bit is on air between
the two, so a radio keeps listening after `end` for the address of a PDU that began by then; a PDU
on another access address in the middle of the window is not received. Written, 20 µs inside each
edge and well outside them.

**PDUs with an invalid CRC.** The tester transmits with another CRC init than the DUT uses. Expect a
negative acknowledgement to the first (LL/CON/PER/BV-15-C) and the end of the event after the second
in a row. Needs a CRC init for the transmission alone, or the reply is received with the wrong one
as well. Written, with `with_crc_error()` marking a PDU of `connection_event`, and a third test for
errors that are not in a row.

**A full receive buffer.** A PDU the DUT's buffer has no room for is not acknowledged, and the event
goes on: the tester's retransmission is acknowledged in a later event, once the buffer was drained.
Written: the device takes nothing from such a PDU and sends its previous PDU again, and a step
drains the buffer with `read_received`, as a link layer does.

**A pending connection event can be cancelled.** In time in the step that scheduled it and from a
timer before its start, too late from a timer at its start, and with nothing pending, as for
advertising events.

### Where the sequence numbers live

**The SN and NESN logic is the buffer's, not the radio's.** The radio takes every PDU it answers
with, its SN, NESN and MD included, from the buffer `link_layer_pdu_buffer()` returns, and keeps no
sequence state of its own. The rig holds a second buffer, and `switch_pdu_buffer` changes to it
between two events, as a link layer with two connections does. The bits of an answer follow from the
PDU just received alone, so what shows where the sequence numbers are kept is what they decide: the
events alternate between two connections, and the second connection's first data is stored once, and
its acknowledgement of the device's data is taken as one. With one buffer for both connections, as a
radio that kept the sequence numbers itself would have it, the test fails in both places.

**The flags are about one event.** The data one connection left unacknowledged is not reported for
the other connection's event, which sent none.

### The flags of `connection_event_events`

The flags describe the event they are reported with, not the connection. One situation for each:

- `unacknowledged_data`: the DUT sends data, and the tester does not acknowledge it;
- `last_received_not_empty`: the tester sends a payload, against an empty PDU;
- `last_transmitted_not_empty`: the DUT's buffer holds data, against an empty buffer;
- `last_received_had_more_data`: the tester's last PDU has MD set;
- `pending_outgoing_data`: more is queued in the DUT's buffer than the event sends;
- `error_occured`: a PDU from the tester with an invalid CRC.

## The pairing toolbox

The functions of `pairing_security_toolbox` have no effect on air and no time in them. They are
called through the rig with their arguments, their results come back in the response, and the host
asserts. No tester, no program, no reset. Written out in radio_tests/toolbox_tests.cpp.

**The specification's vectors.** `f4`, `f5`, `f6`, `g2` and `p256` each have a worked example in
the Core Specification, and `tests/security_manager/test_sm_tests.cpp` already runs them against
the software toolbox. The same inputs go to the DUT, and the same expected outputs are checked.
For `p256` the example gives both key pairs and the shared secret, so it is checked from both
sides.

**Key agreement with an independent implementation.** `generate_keys()` has no expected value,
and a vector can only show that `p256` multiplies correctly with a key it was handed. The
property that matters is that a key pair the DUT generated works: the host generates a pair with
its software toolbox, the DUT generates one, each side computes `p256` from its own private key
and the other's public key, and the two secrets have to be equal. The host also checks that the
DUT's public key lies on the curve, which a hardware port that returns the coordinates in the
wrong byte order fails immediately.

**The nonce.** `select_random_nonce()` is checked for the two things a handful of samples can
show: not zero, and not the same twice. Anything statistical on that few samples proves nothing
and is not attempted.

**Duration is reported, not asserted.** The DUT has no clock and the host's round trip includes
polling, so how long `p256` takes is printed for information, which is enough to notice a port
that takes seconds where another takes tens of milliseconds.

**Only if supported.** Every one of these is decorated with a `precondition` on
`hardware_supports_lesc_pairing`; the legacy functions, once the interface declares them, on
`hardware_supports_legacy_pairing`.

## Housekeeping

**`radio_ready` is reported once.** After a reset, exactly one; on any later collection, none.

**Callback loss is detectable.** Provoke more callbacks than the queue holds without collecting,
then collect and expect a gap in the sequence numbers.

**The reset line works.** Set a session token, reset the DUT through the tester, and require the
token to read zero once the DUT answers again. This runs at the start of every test, so a reset
line that quietly stopped working fails the first test rather than corrupting all of them.

## Optional features and dependent tests

Parts of the interface are optional, such as the 2 Mbit PHY, encryption and the synchronised
user timer, and the rig's `properties()` reports which of them an implementation has. A test of an
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

**The connection event tests could not be written at first.** In the interface the implementation
asks its callbacks for a PDU buffer, and the rig interface said nothing about how the host puts a PDU
into that buffer or reads what was received into it. The rig now does both in the terms of that
buffer: `queue_pdu` fills it, from a step or before the start, and `collect_received` hands over
what the link layer's side of it read.

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

**Running the connection tests found four faults in the nRF52 radio.** An event ended after the
first exchange whatever the MD bits said, as the address of the device's own answer switched the
address interrupt off. A PDU that began shortly before the end of the receive window was cut off, as
the window closed before its address was detected. A PDU the buffer had no room for was acknowledged
and lost. And the mark for unacknowledged data was kept across events, so that one connection's
was reported for another's.

**The tester can only count from a PDU it received.** Every test that only observes gets by
without any origin on the tester. A test in which the tester has to hit a window the DUT opened
cannot, because the window is expressed in the DUT's domain and the tester has no clock origin
of its own that a test could use. The tester needs an operation whose time is relative to a PDU
it received, so that the DUT can mark the origin on air and the tester can count from there. A
connection event counts from the PDU captured last for the first event, and from its own anchor
for every later one.
