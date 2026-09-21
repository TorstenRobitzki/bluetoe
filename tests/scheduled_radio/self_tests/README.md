# Self tests of the test instruments

The tests in this directory test the instruments, not a radio. They are the concepts, the link,
the request and response protocol, the rig and the host's transport, each checked on the host
with dummies and sockets in place of hardware. Nothing here needs a board, and `ctest` runs all
of them like any other unit test, on every platform and every CI job.

The tests of a scheduled radio implementation are a different kind: they run through the
instruments against a real device and live in `../radio_tests/`. See
`documentation/scheduled_radio_test_rig.md`, decision 21.

## What each file covers

| file | subject |
|---|---|
| `concept_tests.cpp` | the C++20 concepts of `scheduled_radio2.hpp` and `link/serial_port.hpp`, each against a model that satisfies it and one that lacks a requirement |
| `link_ring_buffer_tests.cpp` | the single producer, single consumer ring buffer both ends share with their port |
| `link_serialize_tests.cpp` | the byte layout of every type that crosses the wire |
| `link_frame_tests.cpp` | frames on the ring buffers: length prefix, CRC, corrupt and incomplete frames |
| `instrument_dispatcher_tests.cpp` | the device's side of the protocol: opcodes, arguments, results, the status of a request it cannot serve |
| `instrument_reported_queue_tests.cpp` | the queue both instruments report through: order, continuing batch indices, and what a full queue drops and counts |
| `instrument_address_set_tests.cpp` | the acceptance filter set of both instruments |
| `host_proxy_tests.cpp` | the host's side of the protocol, and one call through proxy, frames, buffers and dispatcher |
| `instrument_dut_rig_tests.cpp` | the rig around a scheduled radio: its main loop, the session token, the instrument functions, a radio without a toolbox |
| `instrument_tester_rig_tests.cpp` | the tester's rig: its main loop, the session token, the reset of the device under test |
| `instrument_tester_program_tests.cpp` | the tester as program interpreter: operations run in sequence, the window advances, connection events placed from the anchor before, the received PDUs and their loss |
| `instrument_program_tests.cpp` | the rig as program interpreter: steps on callbacks and the pool of calls they share, times relative to the callback, PDUs queued and read from a step, the two PDU buffers, when a program is finished, the records and their loss |
| `instrument_toolbox_tests.cpp` | the pairing toolbox through the wire, against the software toolbox of the security manager tests, with the Core Specification's vectors |
| `host_central_tests.cpp` | the central a test writes for the tester: the SN, NESN and MD bits of its PDUs, the device's reply to each and its NACK, and the checks of the device's replies |
| `host_transport_tests.cpp` | the host's transport over a stream: a call, a timeout, a corrupt response, a late response |
| `test_tools_timeline_tests.cpp` | what a radio test expects of the PDUs a tester program captured: how an entry is rendered, and which entries count as a difference |
| `test_tools_records_tests.cpp` | what a radio test expects of the device's records: the callbacks and calls of one kind, and the sequence of callbacks a program caused |
| `observed_port.hpp` | the instrumented port both rig tests play the host end of |

A test file is named `<folder>_<subject>_tests.cpp` after the folder of the code it tests, and each
is its own executable, registered in `CMakeLists.txt` with `add_scheduled_radio_test()`.
