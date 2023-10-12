#ifndef BLUETOE_TESTS_SCHEDULED_RADION_TESTER_CONFIG_HPP
#define BLUETOE_TESTS_SCHEDULED_RADION_TESTER_CONFIG_HPP

#include <cstdint>

// QSPI config -> access to external flash memory
static constexpr std::uint32_t ext_flash_d0  = 20;
static constexpr std::uint32_t ext_flash_d1  = 21;
static constexpr std::uint32_t ext_flash_d2  = 22;
static constexpr std::uint32_t ext_flash_d3  = 23;
static constexpr std::uint32_t ext_flash_cs  = 17;
static constexpr std::uint32_t ext_flash_clk = 19;

// UART config -> logging of test results

// UART config -> communication with DUT

#endif
