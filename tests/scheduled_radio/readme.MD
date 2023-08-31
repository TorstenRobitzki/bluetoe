# Testing a Scheduled Radio Implementation

## Basic Idea

The scheduled_radio defines a set of functions that directly or indirectly have a side effect that is visible over the air and in the time domain.

It is essential, that all functions of a scheduled_radio are none-blocking. The basic idea is, that a scheduled_radio implementation is deployed to some hardware. This deployed implementation is named a device under test (DUT). A tester is then connected to the DUT via an interface like an UART. The tester is able to call functions on the scheduled_radio implementation via RPC and to receive calls from the scheduled_radio implementation to the callbacks.

## Tester

The tester is a nRF52 eval board, that will have it's crystal oscillator replaced by a clock source with very high accuracy (e.g. SIT5358AN-FR-33N0-32.000000). The tester will call function on the scheduled_radio implementation and with observe activity over the air and / or interact with the DUT over the air. In addition, the tester will observe calls to the callbacks of the scheduled_radio implementation.

## Device Under Test (DUT)

The device under test combines the implementation of the scheduled_radio and the interface to connect to the tester.

## Protocol (RCP)

The test comes with its own, light weight RPC implementation.

##