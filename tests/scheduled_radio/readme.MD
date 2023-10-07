# Testing a Scheduled Radio Implementation

## Basic Idea

The scheduled_radio defines a set of functions that directly or indirectly have a side effect that is visible over the air and in the time domain.

It is essential, that all functions of a scheduled_radio are none-blocking. The basic idea is, that a scheduled_radio implementation is deployed to some hardware. This deployed implementation is named a device under test (DUT). A tester is then connected to the DUT via an interface like an UART. The tester is able to call functions on the scheduled_radio implementation via RPC and to receive calls from the scheduled_radio implementation to the callbacks.

## Tester

The tester is a nRF52 eval board, that will have it's crystal oscillator replaced by a clock source with very high accuracy (e.g. SIT5358AN-FR-33N0-32.000000). The tester will call function on the scheduled_radio implementation and will observe activity over the air and / or interact with the DUT over the air. In addition, the tester will observe calls to the callbacks of the scheduled_radio implementation.

## Device Under Test (DUT)

The device under test combines the implementation of the scheduled_radio and the interface to connect to the tester.

## Protocol (RCP)

The test comes with its own, light weight RPC implementation.

## Test Framework

The tests themself have to be expressed somehow. Usually a test framework would provide functionality to collect all test that are distributed over several source code files and some mean to express the expected outcomes of a test.

Boost.test: seems to be not very configurable. Have not managed to get a binary that is smaller than 1MByte.

lest: requires exceptions to be enabled

doctest: Fine so far...; ~625k for a single test