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
| `host_proxy_tests.cpp` | the host's side of the protocol, and one call through proxy, frames, buffers and dispatcher |
| `instrument_dut_rig_tests.cpp` | the rig around a scheduled radio: its main loop, the session token, the instrument functions, a radio without a toolbox |
| `instrument_toolbox_tests.cpp` | the pairing toolbox through the wire, against the software toolbox of the security manager tests, with the Core Specification's vectors |
| `host_transport_tests.cpp` | the host's transport over a stream: a call, a timeout, a corrupt response, a late response |

A test file is named `<folder>_<subject>_tests.cpp` after the folder of the code it tests, and each
is its own executable, registered in `CMakeLists.txt` with `add_scheduled_radio_test()`.
