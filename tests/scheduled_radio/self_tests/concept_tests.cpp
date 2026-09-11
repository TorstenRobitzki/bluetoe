/**
 * @file concept_tests.cpp
 *
 * Checks that the concepts of the scheduled radio 2 work are satisfiable and that they
 * reject what they are meant to reject. Each concept is instantiated against a model,
 * the smallest type that satisfies it, and against a model with one requirement removed.
 *
 * A concept that no type satisfies is a specification nobody can meet; one that a type
 * missing a function still satisfies is not checking anything. Both are found here, at
 * compile time, before any implementation exists.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer/scheduled_radio2.hpp>

#include "link/serial_port.hpp"

namespace {

    using namespace bluetoe::link_layer;
    namespace details = bluetoe::details;

    struct model_callbacks
    {
        void radio_ready() {}
        void adv_received( abs_time, const read_buffer& ) {}
        void adv_timeout( abs_time ) {}
        void user_timer( abs_time ) {}
    };

    struct callbacks_without_the_timer
    {
        void radio_ready() {}
        void adv_received( abs_time, const read_buffer& ) {}
        void adv_timeout( abs_time ) {}
    };

    struct model_connection_callbacks : model_callbacks
    {
        struct pdu_buffer {};

        void connection_timeout( abs_time ) {}
        void connection_end_event( abs_time, connection_event_events ) {}

        pdu_buffer& link_layer_pdu_buffer() { return buffer_; }

        pdu_buffer buffer_;
    };

    struct callbacks_returning_the_buffer_by_value : model_connection_callbacks
    {
        pdu_buffer link_layer_pdu_buffer() { return {}; }
    };

    struct model_toolbox
    {
        std::pair< details::ecdh_public_key_t, details::ecdh_private_key_t > generate_keys() { return {}; }
        details::uint128_t select_random_nonce() { return {}; }
        details::ecdh_shared_secret_t p256( const std::uint8_t*, const std::uint8_t* ) { return {}; }
        details::uint128_t f4( const std::uint8_t*, const std::uint8_t*, const details::uint128_t&, std::uint8_t ) { return {}; }
        std::pair< details::uint128_t, details::uint128_t > f5(
            const details::ecdh_shared_secret_t&, const details::uint128_t&, const details::uint128_t&,
            const device_address&, const device_address& ) { return {}; }
        details::uint128_t f6(
            const details::uint128_t&, const details::uint128_t&, const details::uint128_t&, const details::uint128_t&,
            const details::io_capabilities_t&, const device_address&, const device_address& ) { return {}; }
        std::uint32_t g2( const std::uint8_t*, const std::uint8_t*, const details::uint128_t&, const details::uint128_t& ) { return 0; }
    };

    struct model_features
    {
        static constexpr bool           hardware_supports_encryption = true;
        static constexpr bool           hardware_supports_lesc_pairing = true;
        static constexpr bool           hardware_supports_legacy_pairing = false;
        static constexpr bool           hardware_supports_2mbit = true;
        static constexpr bool           hardware_supports_synchronized_user_timer = false;
        static constexpr bool           hardware_supports_link_layer_context = false;
        static constexpr std::size_t    radio_package_overhead = 0;
        static constexpr std::uint32_t  radio_max_supported_payload_length = 255;
        static constexpr std::uint32_t  sleep_time_accuracy_ppm = 20;
    };

    /*
     * Everything scheduled_radio asks for except the toolbox, so that the toolbox can be
     * added or left out. A radio is a template over its callbacks type; the models never
     * call them.
     */
    template < typename CallBacks >
    struct model_radio_base : model_features
    {
        struct ccm_counter_t {};
        struct lock_guard {};

        void run() {}
        void wake_up() {}
        void set_access_address_and_crc_init( std::uint32_t, std::uint32_t ) {}
        void set_ccm_counter( ccm_counter_t&, ccm_counter_t& ) {}
        void set_phy( phy_ll_encoding::phy_ll_encoding_t, phy_ll_encoding::phy_ll_encoding_t ) {}
        void set_local_address( const device_address& ) {}
        bool start_advertising( std::uint32_t, const write_buffer&, const write_buffer&, const read_buffer& ) { return true; }
        bool schedule_advertising_event( std::uint32_t, abs_time, const write_buffer&, const write_buffer&, const read_buffer& ) { return true; }
        bool schedule_connection_event( std::uint32_t, abs_time, abs_time ) { return true; }
        bool cancel_radio_event() { return true; }
        bool schedule_timer( abs_time ) { return true; }
        bool cancel_timer() { return true; }
    };

    template < typename CallBacks >
    struct model_radio : model_radio_base< CallBacks >, model_toolbox {};

    template < typename CallBacks >
    struct radio_without_lesc_pairing : model_radio_base< CallBacks >
    {
        static constexpr bool hardware_supports_lesc_pairing = false;
    };

    template < typename CallBacks >
    struct radio_claiming_lesc_pairing_without_the_toolbox : model_radio_base< CallBacks > {};

    template < typename CallBacks >
    struct radio_with_a_void_cancel : model_radio< CallBacks >
    {
        void cancel_radio_event() {}
    };

    template < typename CallBacks >
    struct radio_without_a_lock : model_radio< CallBacks >
    {
        using lock_guard = void;
    };

    struct model_buffer
    {
        std::size_t free() const { return 0; }
        void push( const std::uint8_t*, std::size_t ) {}
        std::size_t available() const { return 0; }
        void pop( std::uint8_t*, std::size_t ) {}
    };

    struct buffer_with_a_non_const_free : model_buffer
    {
        std::size_t free() { return 0; }
    };

    struct model_wake
    {
        void wake_up() {}
    };

    struct model_port
    {
        model_port( model_buffer&, model_buffer&, model_wake& ) {}
        void start() {}
        void transmit_pending() {}
    };

    struct port_without_a_wake_target
    {
        port_without_a_wake_target( model_buffer&, model_buffer& ) {}
        void start() {}
        void transmit_pending() {}
    };

    struct port_without_a_buffer_constructor
    {
        void start() {}
        void transmit_pending() {}
    };

    using bluetoe::test_rig::byte_ring_buffer;
    using bluetoe::test_rig::wake_up_target;
    using bluetoe::test_rig::serial_port;

    static_assert( scheduled_radio_callbacks< model_callbacks > );
    static_assert( !scheduled_radio_callbacks< callbacks_without_the_timer > );

    static_assert( scheduled_radio_connection_callbacks< model_connection_callbacks > );
    static_assert( !scheduled_radio_connection_callbacks< model_callbacks > );
    static_assert( !scheduled_radio_connection_callbacks< callbacks_returning_the_buffer_by_value > );

    static_assert( lesc_pairing_toolbox< model_toolbox > );
    static_assert( !lesc_pairing_toolbox< model_radio_base< model_callbacks > > );

    static_assert( scheduled_radio_features< model_features > );
    static_assert( !scheduled_radio_features< model_toolbox > );

    static_assert( scheduled_radio< model_radio, model_callbacks > );
    static_assert( scheduled_radio< model_radio, model_connection_callbacks > );
    static_assert( scheduled_radio< radio_without_lesc_pairing, model_callbacks > );
    static_assert( !scheduled_radio< radio_claiming_lesc_pairing_without_the_toolbox, model_callbacks > );
    static_assert( !scheduled_radio< radio_with_a_void_cancel, model_callbacks > );
    static_assert( !scheduled_radio< radio_without_a_lock, model_callbacks > );
    static_assert( !scheduled_radio< model_radio, callbacks_without_the_timer > );

    static_assert( byte_ring_buffer< model_buffer > );
    static_assert( !byte_ring_buffer< buffer_with_a_non_const_free > );

    static_assert( wake_up_target< model_wake > );
    static_assert( !wake_up_target< model_buffer > );

    static_assert( serial_port< model_port, model_buffer, model_wake > );
    static_assert( !serial_port< port_without_a_buffer_constructor, model_buffer, model_wake > );
    static_assert( !serial_port< port_without_a_wake_target, model_buffer, model_wake > );
}

/*
 * The checks above are compile time. This case exists so that the executable reports a
 * result at all.
 */
BOOST_AUTO_TEST_CASE( the_concepts_are_satisfiable_and_selective )
{
    BOOST_CHECK( ( scheduled_radio< model_radio, model_callbacks > ) );
}
