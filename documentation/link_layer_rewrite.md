# The link layer, rewritten step by step

This is the working document of the link layer rewrite: the goals, the constraints the
work has to respect, an analysis of the two axes that shape the design (a dedicated link
layer context, more than one link), the target structure, and the order of the steps.
It is kept while the work goes on and reduced to what a reader of the code needs once it
is done, as the design log of the test rig was.

Every decision in here is a proposal until the maintainer has agreed to it. Agreed
decisions are marked as such.

## Goals

1. **Procedures a client can select.** Every link layer control procedure is a thing of
   its own, chosen by an option of the link layer. A procedure that is not chosen is not
   in the binary: no code, no state, no feature bit.
2. **The known link layer issues fixed.** Fixed while the procedure they belong to is
   moved into its own type, each with a test written against the specification that fails
   before the fix. The list is in the section "Issues".
3. **A radio that provides a link layer context.** The interface of `scheduled_radio2.hpp`
   allows a radio to deliver the callbacks from an interrupt below the radio's priority
   instead of from `run()`. The link layer has to be correct in both cases.
4. **More than one link, and advertising while connected.** Several peripheral links to
   different centrals, and advertising while a link is up, with or without accepting a
   further connection.
5. **The Data Length Update procedure.** LL_LENGTH_REQ and LL_LENGTH_RSP, so that a link
   can carry the 251 byte PDUs the buffer already supports.

## Not goals

- **A central role.** Bluetoe stays a peripheral library. No initiating of connections,
  no scanning. This keeps every procedure at the responding side plus the few requests a
  peripheral may initiate. Agreed.
- **A second link layer next to the first.** The existing `link_layer< Server, Radio,
  Options... >` is changed in place, step by step, with the tests green after every step.
  Agreed, as for the port to the new radio interface.
- **A new radio interface.** `scheduled_radio2.hpp` stays as it is. Where the rewrite
  needs something the interface does not give, that is a finding to be discussed, not a
  change made on the way.
- **An HCI layer.**

## Constraints

These come from the project and are not up for discussion in this work.

- **Footprint is the primary design driver.** One link with today's options has to cost
  what it costs today, in Flash and in RAM, within a few bytes. Every step measures the
  examples with `arm-none-eabi-size` before and after. CI's 256 byte threshold is a last
  resort, not the measure.
- **No dynamic allocation, no exceptions, no RTTI.** The number of links is a compile
  time option. Every buffer and every queue has a compile time size.
- **Everything that can be decided at compile time is.** Which procedures exist, how many
  links, whether there is a link layer context: all options, resolved with the meta type
  machinery of `meta_tools.hpp`, rejected by the existing `static_assert` catch-alls when
  given at the wrong level.
- **The tests are the oracle.** The 636 test cases of `tests/link_layer` are written at
  the PDU level against the simulated radio. They stay valid as long as the type the
  tests instantiate keeps its shape: the link layer template with its options, the
  option types, and the test facing API of the simulated radio. The single link case has
  to pass them unchanged until the multi link step, which is the only step that changes
  what the simulator is.
- **One logical change per commit, each reviewed.** Each step leaves master buildable,
  tested in Debug and Release, the examples cross compiled, and the radio tests run on the
  bench when the radio side changes.
- **Nothing is stored twice for one link.** The radio keeps what its setup functions give
  it: access address and CRC initialiser, the PHY, the encryption keys. Today the link
  layer hands them over when they change and keeps no copy; `connection_parameters` holds
  only what the link layer computes with, the channel map, the window, the interval, the
  latency and the timeout. The single link implementation stays that way. Only the
  implementation for several links keeps the radio's values per link, because it has to
  apply them before every event. Agreed.
- **The nRF52 binding is the reference radio.** It has `hardware_supports_link_layer_context
  = false` today. The final proof of the link layer context needs a radio that has one, so
  the binding gains it, as an interrupt below the radio's priority, when the link layer is
  ready for it.

## Where the two axes really matter

The maintainer asked to find out where a link layer context and where multiple links make
a real difference, so that the design pays for them only there.

### A dedicated link layer context

Without one, the radio delivers its callbacks from inside `run()`, so the radio callbacks,
the procedures, L2CAP, the GATT server, the application's characteristic handlers and the
application's requests all run in one context, one after the other. Nothing is shared and
no lock is needed. With one, the callbacks arrive from an interrupt, and whatever the
application does runs concurrently with them.

Going through the link layer as it is today, the difference shows in exactly four places:

1. **Received data on its way up.** `handle_received_data()` hands every L2CAP PDU to the
   L2CAP layer, which hands it to the GATT server, which calls the application's
   characteristic handlers. Today that happens inside the radio callback. With a link
   layer context it would run application code in interrupt context, and a slow handler
   would hold the link layer context while the next event has to be scheduled.

   This boils down to an ATT SDU queue that both contexts access: the link layer context
   puts a received SDU in, and after a context switch the application context takes it
   out, hands it to the GATT server and puts the response on its way down. The receive
   buffer already holds a PDU until `free_ll_l2cap_received()`, so the queue is the
   ownership of what is already there rather than a copy; the response goes into the
   transmit buffer under `radio_lock_guard`, which is the path notifications take
   already. Agreed as the direction.

   This is the largest single change on this axis, and it is the same code in both
   configurations. Without a link layer context, dispatching from `run()` instead of
   from the callback changes only when in the same thread the work happens.

2. **Requests on their way down.** `disconnect()`, `connection_parameter_update_request()`,
   `phy_update_request()`, `remote_versions_request()` and a queued notification all
   write link layer state from the application context: `disconnect()` sets the
   connection state and starts the procedure timeout directly, the others set flags in
   `procedure_requests` and call `wake_up()`, and the flags are consumed by
   `transmit_pending_control_pdus()` in the callback. With a link layer context these
   writes race with the callbacks. Issues #7 and #151 are this race.

   The pattern that is right in both configurations: a request is recorded in a queue
   under `link_layer_lock_guard`, and the link layer context picks it up at its next
   opportunity. Without a link layer context the lock is empty and the queue is a member
   the callback reads; nothing else differs.

3. **Callbacks on their way up.** `connection_callbacks` already queues every event in a
   ring, calls `wake_up()`, and delivers from the application context. That is the right
   pattern; the ring has to be safe for one producer and one consumer, which is what
   a link layer context makes it. The synchronized connection event callback is the
   exception: it is meant to run at a precise time and stays in the link layer context by
   design.

4. **`run()` itself.** Today it calls the radio once and then loops on
   `event_cancelation_requested_` under the lock. With the dispatch of received data and
   the request queue in place, `run()` becomes: let the radio run, deliver queued
   callbacks, dispatch received data, and the cancellation of a planned event stays as it
   is. The contract of `run()` in the radio interface does not change.

Everything else is context free. A procedure runs in the link layer context, sees the
state of its link exclusively, and never calls the application. The scheduling of the
next event runs there too. So the procedure interface does not need to know about
contexts at all, provided the rule holds that procedures are driven only from the link
layer context and reached from the application only through the request queue.

Where it does not matter, although one might expect it to: the PDU buffer is already
shared between the radio context and the link layer context and already protected by
`radio_lock_guard`; the radio's own state is the radio's concern by the interface's
Decision 17; the timing of the radio events is the radio's, the link layer only has to
schedule the next event before its start, which a dedicated context makes independent of
what the application is doing, and that is the reason to have one.

Cost of supporting both: the request queue, which replaces the flags of
`procedure_requests` and costs the same; a lock that is empty without the context; the
dispatch of received data from `run()`, which is a move, not an addition.

**Proving it.** The unit tests can only show that the two paths are separated: the
simulated radio gets a mode in which the callbacks are delivered as if from another
context, with the application's work interleaved at chosen points. The proof that the
link layer is correct with a real link layer context needs a radio implementation that
has one, and a test on the bench in which the application context is held for several
connection intervals, doing work or sleeping, while the tester on the other side sees the
link stay up and every event answered. That test belongs to the radio tests in
`tests/scheduled_radio/`, with an operation of the rig that occupies the application
context for a given number of intervals. Agreed.

### More than one link

A link is everything that belongs to one connection: the peer's address, access address
and CRC initialiser, channel map, hop increment and event counter, connection
parameters, the peripheral latency state, the PDU buffer, the encryption state and keys,
the PHY, the state of the running procedure, the procedure timeout, and the GATT server's
per connection data, which is already a type parameter, `connection_data_t`, because the
server keeps client characteristic configuration per client. Today all of that is a
member of the link layer, one of each.

Going through the layers, the difference shows in these places:

1. **Ownership of state.** The per link members become one struct that the procedures
   and the shared code act on. The single link implementation holds one, the
   implementation for several links an array of them with a compile time size. For one
   link this is a rename of `this->` into `link.`, and the struct must cost what the
   members cost. What the radio stores is in the struct only in the implementation for
   several links, see the constraints.

2. **The radio setup before every event.** The interface already says it: the setup
   functions, `set_access_address_and_crc_init()`, `set_phy()`, `set_encryption()`, are
   applied by the next scheduling call, and "a single connection link layer calls them
   when a value changes, a link layer with several connections before every action". The
   radio asks for the buffer of the current connection through `link_layer_pdu_buffer()`.
   So the radio side was designed for this; the link layer side has to call the setup
   per event when there is more than one link. That is a few register writes per event
   on the nRF52.

3. **The scheduler.** This is the real difference. With one link the next event is the
   anchor plus the interval, minus what latency allows. With several, the windows of two
   links overlap sooner or later, and something has to decide which link gets the radio
   and which one skips its event. A skipped event is peripheral latency from the
   central's point of view, so the decision has to stay within each link's supervision
   timeout and within the latency the central granted, and a link that has data pending
   or a procedure at an instant must win. Advertising while connected is the same
   decision with an advertising event as one of the candidates. The radio interface has
   one `schedule_connection_event()` and one `schedule_advertising_event()` at a time,
   which is what a scheduler needs: one action, then the next when the callback comes.

   The single link implementation has no scheduler: its next event is what
   `setup_next_connection_event()` computes today.

4. **The advertiser while connected.** Today the state machine is initial, advertising
   or connected. With advertising while connected, advertising is a thing next to the
   links, not a state the link layer is in. A CONNECT_IND while advertising creates a
   link if a link is free, and the advertiser continues or stops depending on that.
   Connectable advertising with all links used is not started.

5. **The user timer.** The radio has one `schedule_timer()`. The synchronized connection
   event callback is defined per connection, so with several links the link layer would
   have to multiplex one timer or restrict the callback to one link. A decision to take
   at the multi link step, not before.

6. **The simulated radio and the fixtures.** The simulator plays one central: one anchor,
   one set of sequence numbers, one response queue, `respond_to( channel, pdu )` and
   `add_connection_event_respond()` on the one link. For several links it has to play
   several centrals with independent anchors and intervals, and a scanner that keeps
   sending scan and connect requests while a link is up. The fixtures grow a link index.
   This is the step where the oracle itself changes, so it comes last.

Where it does not matter: inside a procedure, which acts on its link and nothing else;
in L2CAP and ATT, which are already per connection through `connection_data_t`; in the
radio interface; in the security manager, which is per connection already.

The single link implementation does not pay for any of this. Several links are not one
link with a larger array: they are a second implementation of the link layer, chosen by
the configured number of connections, one by default. The two share what does not
depend on the number of links, the procedures and their engine, the PDU buffer, the
advertiser, the request and the SDU queue, L2CAP and above. What differs, the array, the
scheduler, the setup before every event and the values that needs, exists in the second
implementation only. Agreed.

RAM per link is dominated by its PDU buffer, which is why the number of connections is a
compile time option.

## The target structure

What the link layer is made of when the steps are done. Names are proposals.

- **Two implementations.** `link_layer< Server, Radio, Options... >` selects, by the
  configured number of connections, the single link implementation or the one for
  several links. Everything below is shared unless it says otherwise.

- **`link`.** The struct described above. Owns its PDU buffer, its parameters, its
  latency state, the state of its procedures, and the GATT server's connection data.
  Knows how to compute its next event from its anchor. The single link implementation
  holds one; the one for several links an array, and there the struct also holds the
  radio's setup values, access address and CRC initialiser, PHY and keys.

- **`procedure`.** One type per control procedure. A procedure states the opcodes it
  handles with their sizes, the feature bits it contributes, and provides: a handler for
  a received control PDU on a link, which answers with a PDU or with nothing; a handler
  for the instant, for procedures with one; a handler for the procedure timeout; and,
  for the procedures a peripheral may initiate, a start function driven by the request
  queue. A procedure owns its own state inside the link and nothing outside it.

- **The procedure engine.** Built at compile time from the selected procedures: the
  opcode table, so that an unknown opcode is answered with LL_UNKNOWN_RSP and a known
  opcode with a wrong size is handled as the specification says; the rule that one
  initiated procedure runs at a time per link, with the collision rules of Core
  Specification Vol 6, Part B, section 5.1.1 for a request arriving while one is running;
  the procedure response timeout of 40 seconds. The engine is what the opcode chain in
  `handle_ll_control_data()` and the option mixins that intercept their own opcodes
  become.

- **The request queue.** The one path from the application context into the link layer
  context, under `link_layer_lock_guard`. Replaces `procedure_requests` and the direct
  writes of `disconnect()`.

- **The scheduler.** Only in the implementation for several links. Chooses the next
  radio action among the links and the advertiser, and calls the radio's setup functions
  for it.

- **The advertiser.** Stays what `advertising.hpp` is, but as a thing next to the links
  rather than a state of the link layer.

- **`run()`.** Lets the radio run, delivers queued callbacks, dispatches received data
  to L2CAP, in the application context.

The procedures a peripheral has, with what is known about them:

| Procedure | Opcodes | Today | Selectable |
|---|---|---|---|
| Connection update | LL_CONNECTION_UPDATE_IND | in the chain | no, mandatory to accept |
| Channel map update | LL_CHANNEL_MAP_IND | in the chain | no, mandatory |
| Termination | LL_TERMINATE_IND | in the chain | no, mandatory |
| Version exchange | LL_VERSION_IND | in the chain | no, mandatory |
| Feature exchange | LL_FEATURE_REQ, LL_FEATURE_RSP | in the chain | no, the response is mandatory |
| Unknown and reject | LL_UNKNOWN_RSP, LL_REJECT_IND, LL_REJECT_EXT_IND | in the chain | no, part of the engine |
| Encryption | LL_ENC_REQ/RSP, LL_START_ENC_REQ/RSP, LL_PAUSE_ENC_REQ/RSP | security mixin | yes, with the security manager |
| Ping | LL_PING_REQ, LL_PING_RSP | in the chain | yes; required with encryption (#105) |
| Connection parameters request | LL_CONNECTION_PARAM_REQ/RSP | in the chain | yes (#9) |
| PHY update | LL_PHY_REQ/RSP, LL_PHY_UPDATE_IND | PHY mixin | yes, with a 2M radio |
| Data length update | LL_LENGTH_REQ, LL_LENGTH_RSP | not implemented | yes, new |

The mandatory procedures are mandatory: they are always in, and no option leaves one
out. Which procedures those are is read against Vol 6, Part B, sections 4.6 and 5.1 when
the engine is built; the table records the intent. Agreed.

## Issues

The open issues that belong to this work, grouped by where they get fixed.

- **Engine:** #123 protocol collision, #125 procedure timeout, #126 invalid control PDUs,
  #131 overlapping procedures, #115 unexpected PDU during encryption start.
- **A procedure:** #105 ping not sent, #118 PHY instant in the past, #122 PHY update
  initiated by us, #124 parameter check of LL_CONNECTION_PARAM_REQ, #129 asymmetric PHY
  request, #130 lost connection after PHY update, #9 connection parameter update optional.
- **Link and scheduler:** #119 connection timeout with invalid CRCs, #120 latency before
  the first acknowledgement, #116 disconnect on invalid MIC, #132 LL/CON/ADV/BI-01-C.
- **Contexts:** #7 disconnect, #151 advertising count from two contexts.
- **Buffers:** #80 L2CAP fragmentation, #32 and #40 buffer size defaults.
- **Left as decided:** #84 stays open; the simulation's `run()` is not made to return per
  event, a caller relying on that is the bug.

Each issue is fixed in the step that touches its code, with a test first.

## The steps

Each step is a series of small commits on master, each green.

1. **The link struct.** The per link members moved into it, held as an array of one.
   Measured: no change in size.
2. **The request queue and the SDU queue.** Requests from the application go through
   the request queue; received ATT SDUs go through the SDU queue and are handled from
   `run()`. Fixes #7 and #151. This is where the link layer becomes correct for a radio
   with a link layer context, and the simulated radio gets the mode that delivers as if
   from another context. The bench test with a real link layer context follows once the
   nRF52 binding provides one; that is a step of its own, on the radio side.
3. **The procedure engine and the mandatory procedures.** The opcode chain becomes the
   engine; termination, version, feature exchange, channel map, connection update, and
   the unknown and reject handling become procedure types. The engine fixes go in here.
4. **The optional procedures, one at a time.** Ping, connection parameters request,
   encryption, PHY update. Each becomes selectable, and the issues of each go in with it.
   The first one that is left out of an example proves that leaving it out costs nothing.
5. **The Data Length Update procedure.** Written new on the engine. Proves that a
   procedure is added without touching the others.
6. **The implementation for several links.** Selected by the configured number of
   connections: the array of links, the scheduler, the setup before every event. The
   simulator plays several centrals and the fixtures get a link index. The single link
   implementation and its tests are untouched by this step.
7. **Advertising while connected.** The advertiser as a candidate of the scheduler, a
   CONNECT_IND while a link is up creating a further link, tested with the simulator's
   scanner. Proposal: this belongs to the implementation for several links only, since
   it is the scheduler that makes it possible.

Steps 1 to 5 keep every existing test as the oracle and shape the single link
implementation. Steps 6 and 7 add the second implementation next to it.

## Decisions to take

- How the number of connections is expressed: proposal
  `bluetoe::link_layer::max_connections< N >`, default one, which selects the
  implementation.
- Whether the single link implementation advertises while connected, or only the one for
  several links. Proposal: only the one for several links.
- Whether the synchronized connection event callback is limited to one link or
  multiplexed. To be decided at step 7.
- The names above.
