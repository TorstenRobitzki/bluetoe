#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_DUMMY_RADIO_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_DUMMY_RADIO_HPP

/**
 * @file dummy_radio.hpp
 *
 * A scheduled radio for the host, where none exists: what lets the host instantiate the
 * rig template, see dut_functions.hpp and documentation/scheduled_radio_test_rig.md,
 * decision 20. Nothing in it ever runs; the rig's unit tests derive an instrumented
 * version to observe the rig.
 */

#include <bluetoe/link_layer/scheduled_radio2.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief a radio that exists only so that the rig can be instantiated on the host
     *
     * It satisfies scheduled_radio and does nothing. It claims every feature, so that
     * every wrapper the rig has is in the host's list.
     */
    template < typename CallBacks >
    class dummy_radio
    {
    public:
        static constexpr bool           hardware_supports_encryption                = true;
        static constexpr bool           hardware_supports_lesc_pairing              = true;
        static constexpr bool           hardware_supports_legacy_pairing            = true;
        static constexpr bool           hardware_supports_2mbit                     = true;
        static constexpr bool           hardware_supports_synchronized_user_timer   = true;
        static constexpr bool           hardware_supports_link_layer_context        = false;
        static constexpr std::size_t    radio_package_overhead                      = 0;
        static constexpr std::uint32_t  radio_max_supported_payload_length          = 251;
        static constexpr std::uint32_t  sleep_time_accuracy_ppm                     = 250;

        struct ccm_counter_t {};
        struct lock_guard {};

        void run() {}
        void wake_up() {}

        void set_access_address_and_crc_init( std::uint32_t, std::uint32_t ) {}
        void set_ccm_counter( const ccm_counter_t&, const ccm_counter_t& ) {}
        void set_phy( link_layer::phy_ll_encoding::phy_ll_encoding_t, link_layer::phy_ll_encoding::phy_ll_encoding_t ) {}
        void set_local_address( const link_layer::device_address& ) {}

        bool start_advertising( std::uint32_t, const link_layer::write_buffer&, const link_layer::write_buffer&, const link_layer::read_buffer& )
        {
            return false;
        }

        bool schedule_advertising_event( std::uint32_t, link_layer::abs_time, const link_layer::write_buffer&, const link_layer::write_buffer&, const link_layer::read_buffer& )
        {
            return false;
        }

        bool schedule_connection_event( std::uint32_t, link_layer::abs_time, link_layer::abs_time )
        {
            return false;
        }

        bool cancel_radio_event()
        {
            return false;
        }

        bool schedule_timer( link_layer::abs_time )
        {
            return false;
        }

        bool cancel_timer()
        {
            return false;
        }

        std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > generate_keys()
        {
            return {};
        }

        bluetoe::details::uint128_t select_random_nonce()
        {
            return {};
        }

        bluetoe::details::ecdh_shared_secret_t p256( const std::uint8_t*, const std::uint8_t* )
        {
            return {};
        }

        bluetoe::details::uint128_t f4( const std::uint8_t*, const std::uint8_t*, const bluetoe::details::uint128_t&, std::uint8_t )
        {
            return {};
        }

        std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > f5(
            const bluetoe::details::ecdh_shared_secret_t&, const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t&,
            const link_layer::device_address&, const link_layer::device_address& )
        {
            return {};
        }

        bluetoe::details::uint128_t f6(
            const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t&,
            const bluetoe::details::io_capabilities_t&, const link_layer::device_address&, const link_layer::device_address& )
        {
            return {};
        }

        std::uint32_t g2( const std::uint8_t*, const std::uint8_t*, const bluetoe::details::uint128_t&, const bluetoe::details::uint128_t& )
        {
            return 0;
        }

    };
}
}

#endif
