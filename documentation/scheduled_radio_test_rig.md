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
of its own choosing and records that `abs_time`; the tester timestamps the arrival with its accurate
clock. Two such transmissions give the same interval in both domains, once as requested and once as
observed, and comparing the two is the measurement. The domains never have to be related to each
other for that. Where drift matters, roughly five microseconds per hundred milliseconds at the worst
clock error the specification permits, it enters the tolerance of the interval rather than a
synchronisation.

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
denying the rig a timebase forces every test to obtain time through the timestamps carried by the
callbacks, which are exactly the contract under test. A rig with its own clock would let tests
establish time without ever exercising the times the interface hands out.

## 6. Half duplex, with the host always initiating

The device under test never speaks unsolicited. Every request is answered by exactly one response,
which may take as long as the call takes. Callbacks are queued and collected by polling, and the
host polls state, `program_finished()` and the collections, never the result of a call.

This buys the property that matters most: the device under test never waits on the host. Its main
loop becomes "answer a complete request if one is buffered, otherwise call `run()`", with no
blocking read anywhere, so it can never be parked in a receive while a radio event needs servicing.
That failure mode would quietly invalidate the timing results the rig exists to produce.

It also removes any question of both ends transmitting at once, lets one implementation serve both
endpoints, and keeps the rig contract portable to transports that are genuinely half duplex. Since
all timing information travels in the payload rather than in the arrival of a message, polling
costs bench time and nothing else.

**Amended.** The first form of this decision said that a call with a result becomes a call followed
by polling for that result. That prescribed a protocol shape in the name of a property the frames of
decision 16 already provide, and the one long call there is shows the shape buys nothing. `p256()`
takes hundreds of milliseconds in the rig's application context, and for that time neither the link
nor `run()` is serviced whatever the protocol, because they live in the same loop; an "accepted"
response followed by polls does not shorten that, it only sits the first poll in the receive buffer
until the computation ends, and doubles every call for it. The delay to `run()` is not a rig
artefact either: it is what a single context radio does when the security manager computes a key
during a connection, and a radio with its own link layer context is what keeps the events on time
meanwhile. A test of that property issues the call, lets it take its time, and checks that the
events kept their interval, which needs the single response form. A dead device is still detected,
by the host's timeout on the response.

**Rejected:** the two phase form, for the reason above. If a computation ever moves into a context
of its own, the state polling pattern of decision 14 covers it: start it, poll a status function.

## 7. The callback queue must make loss detectable

Callbacks occur while the host is not listening, so they are queued with their timestamps, and the
queue can overflow.

A silently dropped callback would corrupt a test in the most misleading way available, by turning
"the radio did not call me" into a passing negative assertion. Every record therefore carries a
sequence number, and the host treats a gap as a void test. A separate overflow flag was considered
and dropped: it would be a second way of saying what the numbers already say.

The depth is not part of the contract. Under decision 14 the host collects once, after a program
finished, so the depth bounds the length of a program and nothing else; a program that outgrows it
shows up as a gap, and the remedy is a larger array. The host never needs to know the number.

## 8. A session token in every response

The host gives an instrument a random, non-zero token, and every response from then on echoes it.
The variable holding the token is zeroed at startup like any other, so a restart, however caused,
reads back as zero.

Under decision 6 the instrument may not announce itself after booting, and a value checked on every
exchange is better than an announcement would have been: it is checked continuously rather than
only when a reset was expected, and it detects a restart that happened in the middle of a
sequence, which is the failure mode that occurs while bringing up a new radio. A stale instrument
from an earlier run cannot know the token either.

The earlier form of this decision was a boot counter that increments on every reset. The token is
better on two counts. A counter has to survive a hardware reset, so it needs a word of RAM in a
section the startup code does not clear, which is toolchain and part specific and fails silently;
the token needs the opposite, a variable that is cleared, which every platform does unasked. And
the token proves that the reset line works: the host sets a token, resets, and requires zero. A
counter that does not increment looks the same as a counter that was never implemented.

The fixture therefore reads: set a token, reset through the tester, poll until the device answers
and require the token to be zero, set a new token, wait for `radio_ready`. The tester gets the
same treatment, since it can crash too.

## 9. The rig contract is a separate artefact from the radio interface

The rig has its own small contract, stated independently of any particular hardware: a hardware
reset input with a bounded worst case time to ready, a serial link with defined framing, an
identity and version report, and the session token of decision 8.

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
2. The rig and the serial port on the nRF52, with the tests of the pairing toolbox. These need no
   tester and nothing on air.
3. A narrow but real radio on the nRF52: `radio_ready()`, `start_advertising()`, an advertising
   event scheduled relative to the callback that ends it, and the callbacks these events
   produce; together with the tester and its validation, which is a precondition of the first
   timing assertion and of nothing before it.
4. The tests, broadened together with the implementation.
5. The link layer.
6. The link layer context of decision 17.

The toolbox comes first because its functions are the simplest remote calls the interface has,
arguments in and a result out, with no time, no callback, no program and no tester, and because
their correct answers are known: the Core Specification's test vectors already run against the
software toolbox in `tests/security_manager/`. Every piece of the rig, the link, the framing, the
session token, the serialisation of every argument type, and the call-then-poll pattern of decision
6 on a computation that takes long enough to need it, gets its first exercise on a function whose
result can be checked, before anything with timing is attempted.

The radio comes before the timing tests because nothing yet demonstrates that the interface is
implementable with the timing it promises, and writing those tests against an unimplemented radio
risks baking in assumptions the hardware will not honour. It does not have to be the whole radio.
Scheduling one advertising event relative to a callback, with the time reported back from the
hardware capture, exercises the claim the whole design rests on, and it is a small part of the
work.

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
context, the specification names the context each function and callback belongs to, the mutual
exclusion primitive the radio provides, and what `run()` means. Naming those now makes the later
split a change of implementation rather than of contract. Decision 17 does this.

## 12. The comparison window of `abs_time`

`abs_time` values form a ring, so the comparison operators do not answer a general question about
ordering. What they answer is a question about proximity: given a time a caller wants to schedule
an action for, is that time already in the past, or so close to now that the hardware cannot be set
up in time?

Beyond `max_distance` the answer is definitively "neither", which is the correct and safe answer to
that question. A time that far away is certainly not in the near past and certainly not imminent.
So the operators are not fragile outside the window; they are simply reporting "far", and comparing
values that are far apart is legitimate use rather than a precondition violation. That is worth
stating in the header, because an operator called `<` invites a reader to expect an ordering, and
because it rules out asserting on the distance, which would fire on correct code.

The design considerations document already gives this principle in section 3.1.1: a time computed
by the link layer can land in the past only by the few milliseconds that interrupts and the
calculation itself cost, and never near the maximum waiting time, so near past and far future are
distinguishable. What it does not do is connect that principle to a value.

The constant has to sit between two bounds. It must be far larger than those few milliseconds of
jitter, which is easy. And it must be at least as large as the greatest distance at which a caller
still needs a truthful answer for a time that really is ahead of now, because beyond the window
such a time reads as "far" rather than "in the future". The core specification caps the supervision
timeout at 32 seconds and requires it to exceed twice the latency times the interval, so the
largest legal gap between two connection events a peripheral must handle is just under 16 seconds.
The implemented 16.777216 seconds covers that with about five percent to spare, which is a tighter
fit than it looks and worth recording next to the constant.

The 2000 second figure in the document sizes the representation rather than the window. Both are
sound, but they answer different questions and should not be conflated.

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

## 14. A test is a program on each instrument, started together and collected afterwards

The host does not drive the instruments call by call while a test runs. It loads a program into
each, a sequence of steps the instrument executes on its own, starts both, waits until they report
that they finished, and then collects what was recorded on either side and asserts over it.

On the device under test a step is "on this callback, make these calls", with every time expressed
relative to the time that callback carried. The rig executes the step inside the callback, and
records the callback, the calls it made with their resolved arguments, and their return values. On
the tester a step is one of its operations, run for a stated duration from the moment the previous
one ended. Nothing on the tester is placed at a point in time: the tester has no origin that means
anything to a test, and the origin of the device under test only becomes visible to it when a PDU
arrives, so an operation that has to transmit at a particular moment will be expressed relative to
a received PDU.

The order in which the host does this is fixed: reset the device under test, wait until it reports
`radio_ready`, load the tester's program, load the device's program, start the tester, start the
device. Readiness is verified before anything is loaded, so a device that does not boot is reported
as that rather than as an empty result, and the two programs start one round trip apart rather
than one boot apart. That is what allows the tester's listen durations to be computed from the
program they accompany instead of being sized for a boot, which would have to be padded and would
be paid on every test.

The first step of a device program runs on `start`, when no time exists yet on the device. It
therefore begins with `start_advertising()` of decision 15, whose callback carries the time every
later step is computed from. Every test starts with the device transmitting, and the
first transmission is the origin for both sides.

The first version of the test sketches drove the device under test from the host, one call at a
time, and every timed test had to begin by reading the device's clock, sending it back a time far
enough ahead to survive two serial round trips and a polling loop, and hoping. The margin that made
that work was a property of the serial link and the host, not of the radio, and yet it sat in every
test as a number. Worse, an interval between two events had to be assembled from a time the device
reported, a time the host computed, and the latency in between.

With programs, the only time that ever crosses the link is a recorded one. The device under test
reacts to a callback in microseconds, so a step can ask for something a few milliseconds ahead and
the margin that remains is the implementation's own. That margin is not a number to state: a test
that runs connection events at the shortest interval the specification allows shows it is small
enough, which is the only thing anyone needs to know about it. The tests also read as what they
are: what each side does, then what was expected to come out.

**Rejected:** compiling each test's device side into the firmware and selecting it by number. It
keeps the timing just as well, but it moves half of every test onto the device, which is the
arrangement decision 1 argued against. A table of steps is small enough that the rig can interpret
it, and the vocabulary of steps is the interface itself.

## 15. The interface has no function that returns the current time

`scheduled_radio2` had a `time_now()`. It is removed. Every `abs_time` the implementation hands
out is the time of something that happened on the radio, given to the callback that reports it,
and every time a caller passes in is derived from one of those: each from the callback that ended
the previous action.

For the same reason `radio_ready()` carries no time. Nothing has happened on the radio when it is
called, and the caller's first action needs no time, so a timestamp there would only oblige an
implementation to run a clock before the radio was ever used.

The reason is what the hardware can honestly promise. Between radio events a low power
implementation runs only its low frequency clock; the high frequency clock, which is what the
microsecond resolution of `abs_time` comes from, is started for an event and stopped afterwards. A
`time_now()` called in between would either return a coarse value dressed up in a fine
representation, or force the high frequency clock to run for the benefit of a query, which is a
power cost the link layer never asked for and the interface cannot see. Inside a callback neither
problem exists: the radio has just produced a timestamp, so it demonstrably has a clock.

What replaces it is `start_advertising()`, which names no time and schedules one advertising
event as soon as possible. It is deliberately not an overload of `schedule_advertising_event()`,
because its contract differs: it cannot be too late. A sequence of radio actions starts with it,
and continues with timed actions relative to the callbacks the earlier ones produced; the name
says where the sequence begins, not that the radio keeps advertising on its own. The places where a caller holds
no usable time are exactly the places where it does not want one: the first event after
`radio_ready`, and every return to advertising after a connection ended or after advertising was
switched on while the radio was idle. The first step of every test program is the same case.

Two things confirmed that nothing else is lost. Decision 14 made every test express its times
relative to callbacks, and none of them needed a free standing clock afterwards. And a link layer
computes its times the same way, from the anchor a connection event reported or from the time a
`CONNECT_IND` was received, both of which arrive as callback arguments. The scheduling functions
still have to answer whether a requested time is already too close, which is what
`abs_time::is_in_near_past()` is for; the implementation reads its own clock to do that, and that
clock stays private to it.

The instruments follow the same rule: neither the rig nor the tester offers a time function to the
host, because under decision 14 the host has no use for one.

## 16. An instrument is three parts, and the platform dependent one is a byte port

An instrument consists of the scheduled radio implementation, which is the subject; the rig,
which is the same on every platform; and a serial port, which is written once per platform. The
rig is everything that is not the radio and not the port: framing, the request and response
protocol, the program interpreter, the records, the session token.

The port's contract is event driven and stated in `tests/scheduled_radio/link/serial_port.hpp`. The
port is constructed on two ring buffers the rig owns, pushes what it receives into one and pops
what it transmits from the other, both from a context below the radio's priority. It owns no
buffer and makes no decision, so on most parts it is a UART setup and an interrupt handler with
two branches.

The link does not lose data, because nothing forces it to. Each ring buffer answers how much it
can take and how much it holds, as lower bounds that stay true without further synchronisation
between the two contexts, and the port uses the first answer to hold the host off: USB CDC by not
acknowledging, a UART with flow control by deasserting RTS. A port with no means of back pressure
is covered by the protocol itself, since a request never exceeds the receive buffer and the host
never sends the next before it has the answer. Designing for loss would have added a counter, a
checksum failure and a host side interpretation of both, to handle a case that does not have to
exist.

What is genuinely platform dependent turns out to be small: the UART, the two moments "a byte
arrived" and "a byte can be sent", their interrupt priority, and whatever makes the reset input
work. Everything else exists once.

The split buys more than tidiness. The rig compiles on the host against a fake port and the
simulated radio, so the program interpreter, the framing, the sequence numbers and the loss
detection can be unit tested with the tooling already in `tests/` before anything is flashed. A
bug in the rig presents as a bug in the radio, which makes the rig the part that most needs to be
testable on its own. The tester shares the rig's link code, so this also answers part of the open
question on validating the tester.

Two things the split settles as requirements of the rig rather than of a port: framing is length
prefixed with a checksum, decided once; and the rig's main loop is "if a complete request is
buffered, answer it; then call `run()`", where answering never waits for the port. A response that
does not fit into the transmit buffer waits for the next iteration.

**Rejected:** a polled `read()`/`write()` pair. It looks simpler still, but it makes every port
responsible for buffering whatever arrives between two polls, so the buffer is written once per
platform, and whether bytes are lost depends on how long `run()` takes on that platform, which
is exactly the variability the rig should not have.

**Amended.** The port is constructed on a third thing, a reference to an object with `wake_up()`,
and calls it after it pushed received bytes. The rig's main loop sleeps in the radio's `run()`, and
decision 17 makes `wake_up()` the only guaranteed way to make that return; the port is the
"interrupt of the application" that decision speaks of. Without this a request could sit in the
receive buffer until the radio returned for a reason of its own. The rig passes the radio.

## 17. Three contexts, and one meaning of `run()`

Three contexts exist, named by who lives in them. The *radio context* is the implementation's own
interrupt context. The *link layer context* is where the radio delivers its callbacks and where the
scheduling functions are called from. The *application context* is where `run()` executes and
where the GATT layer delivers its callbacks. A radio without a context of its own delivers the
callbacks from inside `run()`, so the last two are the same; a radio that advertises
`hardware_supports_link_layer_context` provides the link layer context itself, as an interrupt
below the radio's priority and above the application's. The contract is identical in both cases.

`run()` has one meaning everywhere: sleep until there is something for the application context to
do, then return. `wake_up()`, callable from any context including interrupts, guarantees that it
returns: the link layer context calls it after receiving something the application has to process,
an interrupt of the application calls it to get the main loop going. The guarantee runs one way
only. `run()` may also return for reasons of its own that the contract does not enumerate, so a
caller never concludes from a return that `wake_up()` was called; it looks at its state, finds
nothing to do, and calls `run()` again. Each layer's `run()` forwards
to the one below and does its own application context work when the call comes back; the link
layer's does the L2CAP and ATT processing and delivers the GATT callbacks. An application loops
over the topmost `run()` and never learns how many contexts the radio has. The call chain defines
the context, not who is at its top.

Every function and callback of the interface belongs to one of four words:

| | context |
|---|---|
| `run()` | application |
| `wake_up()` | any, including interrupts |
| `lock_guard` | application |
| `start_advertising()` | application or link layer; the radio makes it safe |
| `set_*`, `schedule_*`, `cancel_*` | link layer |
| the constants | any |
| the pairing toolbox | any, reentrant |
| the six callbacks | link layer |
| `link_layer_pdu_buffer()`, the white list check | radio; return immediately |

The rule that keeps this small is that the radio's state is touched from one context only. Every
scheduling call is made from the link layer context, so the interface needs no mutual exclusion
for its own sake. The one exception is `start_advertising()`: switching advertising on happens in
the application context while the radio is idle, and rather than make the caller prove that
nothing is in flight, the radio is made responsible for that, since only it can see.

`lock_guard` exists for the link layer, not for the radio. It excludes the link layer context and
protects the state the link layer shares between its two halves: data on its way from the
application to the PDU buffer, and received data on its way up. How the link layer splits its work
across the line, and in particular where L2CAP reassembly happens, is step 4 of decision 11 and not
the radio's concern; the radio provides the lock and `wake_up()`, and the PDU buffer is the
structure already built to be written from one side and read by the radio.

Two callbacks are not in the link layer context and the interface says so loudly.
`link_layer_pdu_buffer()` is called between the PDUs of a connection event, with the inter frame
space to spare, so it runs in the radio context and returns at once; the buffer it returns is used
from the radio context while the link layer fills and drains it from the link layer context. The
white list check during an advertising event is the same case. It is referred to in the
documentation of `schedule_advertising_event()` but not declared among the callbacks, which is a
gap to close, not part of this decision.

The pairing toolbox belongs to neither side. Its functions are pure computations on their
arguments and long, hundreds of milliseconds for a point multiplication on a small core. Running
them in a context that preempts the application would stall it for that long, so their contract is
"any context, reentrant", and where the security manager does its slow work is deferred to the
link layer.

For the rig, a program step runs in the link layer context, exactly where the link layer's
equivalent would, and the record queue is written there and collected from the application
context, single producer and single consumer like the byte buffers of decision 16.

**Rejected:** a `request_callback()` primitive by which the application context asks for one
invocation of a callback in the link layer context, so that it can schedule from there. It looked
necessary until the cases were listed: a notification, an `LL_TERMINATE_IND`, a parameter update
request all reach the air through the PDU buffer and the connection event that is already
scheduled, with no scheduling call at all. The only application context need is starting
advertising while idle, which the rule above covers.

**Rejected:** allowing the scheduling functions from the application context while a `lock_guard`
is held. It would work, but it makes the caller responsible for a collision that only the radio
can see, and it lets the lock's purpose blur from "the link layer's state" into "anything".

## 18. The C++ interfaces are concepts; the wire contracts stay prose

`scheduled_radio2.hpp` and `serial_port.hpp` state their requirements as C++20 concepts:
`scheduled_radio_callbacks`, `lesc_pairing_toolbox`, `scheduled_radio_features`, `scheduled_radio`,
`byte_ring_buffer` and `serial_port`. The instrument contracts in `instrument.hpp`, `dut_rig.hpp`
and `tester.hpp` stay as they are.

The distinction is whether anything is generic over the type. A radio has several implementations,
the nRF52, the simulated radio, the next port, and two generic consumers, the link layer and the
rig. A serial port has one implementation per platform and one consumer. For those a class
declaration used as documentation is checked by nobody until a link layer is instantiated against
a port and the compiler reports a missing member somewhere inside the link layer's code; a concept
checks at the point of use and names the missing requirement. The conditional parts fit as well:
the toolbox is required only where `hardware_supports_lesc_pairing` is true, which a concept
states directly and a class declaration could only say in a comment. The instruments, by
contrast, exist once each and are reached over a link; nothing is generic over them as C++ types,
and their requirements, the token in every response, the sequence numbers, are wire behaviour that
no concept can express.

A concept checks syntax. Everything decision 10 and decision 17 ask for, the observable effect,
the tolerance, the context, is semantics and stays as prose next to each requirement, which is
what the rig checks. What changes is that the two headers now have to compile: a concept that
does not is worthless. `tests/scheduled_radio/concept_tests.cpp` instantiates each concept
against a model, the smallest type that satisfies it, and against a model with one requirement
removed, so that a concept nobody can satisfy and a concept that checks nothing are both found at
compile time. The test target sets C++20 itself, as decision 13 requires.

One consequence for the shape of the interface. It was a class template
`scheduled_radio2< CallBacks, Options... >` with the callbacks and the toolbox as base classes. A
concept is over a type, so the interface becomes `scheduled_radio< Radio >`, and how an
implementation receives its callbacks and its options is the implementation's business, not the
interface's.

**Amended.** `scheduled_radio` is a concept of a template and a type, `scheduled_radio< Radio,
CallBacks >`, not of a type. A radio is a template over the type it delivers its callbacks to,
because that is the only way it can call them without indirection, and it reaches that type through
the base class relation, as the bindings do today. A concept over the instantiated type alone hid
that, and it left the callbacks type unchecked: neither side of the pair can check the other in its
own declaration, since the radio sees its parameter incomplete when it is instantiated as a base
class, and the callbacks type cannot name itself in a constraint. The consumer that owns the pair
checks both at once; the rig and the link layer, which both pass themselves, do so in a member
function.

In the same step `scheduled_radio_callbacks` is split: the callbacks of advertising and of the
timer, which every callbacks type provides, and `scheduled_radio_connection_callbacks`, required of a
type that schedules connection events. The rig's link layer half satisfies the first before it
implements connections, and the concept tracks what exists instead of demanding a placeholder PDU
buffer.

## 19. The wire carries the rig's interface, not the radio's

What is serialised between the host and the device under test are calls to the rig: its own
functions, the program actions, and one wrapper per toolbox function. The radio's interface is
never on the wire.

The reason is the pointer arguments of the toolbox. `f4()` takes `const std::uint8_t*` for the two
public key coordinates, and stays that way because a coordinate is a part of a larger key and an
array parameter would force a copy on every call. A serialiser cannot size a pointer from the
signature. Rather than teach it sizes per function, the rig's wrapper takes `std::array` parameters,
which carry their size, and forwards to the radio with `.data()`. The same move settles the other
awkward cases without a mechanism: a program action is a rig function whose time parameter is a
`delta_time` by declaration, and the buffers of an advertising event are rig functions taking bytes
the rig copies into its own storage. Every serialised function is then an ordinary C++ function
whose parameters the serialiser understands from the signature alone, and the mapping in
`dut_rig.hpp` is code rather than convention.

The serialiser itself is type driven: fixed width integers little endian, arrays and tuples element
by element, `abs_time` and `delta_time` as their microseconds, a variable length byte sequence with
a length prefix. Deserialising from a truncated frame fails rather than reads past the end, and
serialising into a full buffer fails rather than truncates, both reported as a result rather than
thrown, because the device has no exceptions and a malformed request is a link error the host
should see, not a crash.

## Open questions

- How the tester itself is validated. Its timestamps and its T_IFS response are the measurement, so
  an error there presents as a fault in the device under test. Checking it against a known good
  device or against a sniffer has to happen before the first timing assertion is believed; decision
  11 places it there, and defers the how.
