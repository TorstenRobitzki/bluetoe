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

#include <bluetoe/scheduled_radio2.hpp>

#include "host/dummy_radio.hpp"
#include "link/serial_port.hpp"

namespace {

    using namespace bluetoe::link_layer;

    /*
     * The callbacks a scheduled radio delivers to, in parts, so that a model can leave one
     * out and fail the concept for that reason alone.
     */
    struct advertising_callbacks
    {
        void radio_ready() {}
        void adv_received( abs_time, const read_buffer& ) {}
        void adv_timeout( abs_time ) {}
    };

    struct timer_callback
    {
        void user_timer( abs_time ) {}
    };

    struct connection_callbacks
    {
        struct pdu_buffer
        {
            read_buffer allocate_receive_buffer() { return {}; }
            reception_result received( read_buffer ) { return {}; }
            write_buffer next_transmit() { return {}; }
            bool pending_outgoing_data_available() { return false; }
        };

        void connection_timeout( abs_time ) {}
        void connection_end_event( abs_time, connection_event_events ) {}

        pdu_buffer& link_layer_pdu_buffer() { return buffer_; }

        pdu_buffer buffer_;
    };

    struct software_filter
    {
        bool is_in_acceptance_filter( const device_address& ) { return true; }
    };

    struct model_callbacks : advertising_callbacks, timer_callback, connection_callbacks, software_filter {};

    struct callbacks_without_the_timer : advertising_callbacks, connection_callbacks, software_filter {};

    struct callbacks_without_the_connection_callbacks : advertising_callbacks, timer_callback, software_filter {};

    struct callbacks_without_the_acceptance_filter : advertising_callbacks, timer_callback, connection_callbacks {};

    struct callbacks_returning_the_buffer_by_value : model_callbacks
    {
        pdu_buffer link_layer_pdu_buffer() { return {}; }
    };

    struct buffer_without_next_transmit
    {
        read_buffer allocate_receive_buffer() { return {}; }
        reception_result received( read_buffer ) { return {}; }
        bool pending_outgoing_data_available() { return false; }
    };

    struct callbacks_with_an_incomplete_buffer : model_callbacks
    {
        buffer_without_next_transmit& link_layer_pdu_buffer() { return incomplete_; }

        buffer_without_next_transmit incomplete_;
    };

    struct model_acceptance_filter
    {
        bool add_to_acceptance_filter( const device_address& ) { return true; }
        bool remove_from_acceptance_filter( const device_address& ) { return true; }
        void clear_acceptance_filter() {}
        std::size_t acceptance_filter_free_size() const { return 0; }
    };

    struct acceptance_filter_without_clear
    {
        bool add_to_acceptance_filter( const device_address& ) { return true; }
        bool remove_from_acceptance_filter( const device_address& ) { return true; }
        std::size_t acceptance_filter_free_size() const { return 0; }
    };

    /*
     * The radio models are the host's dummy radio, in the parts the concepts are in, so
     * that a model can leave the toolbox out or claim what it does not have.
     */
    using bluetoe::test_rig::dummy_features;
    using bluetoe::test_rig::dummy_toolbox;
    using bluetoe::test_rig::dummy_radio_base;
    using bluetoe::test_rig::dummy_radio;

    template < typename CallBacks >
    struct radio_without_lesc_pairing : dummy_radio_base< CallBacks >
    {
        static constexpr bool hardware_supports_lesc_pairing = false;
    };

    template < typename CallBacks >
    struct radio_claiming_lesc_pairing_without_the_toolbox : dummy_radio_base< CallBacks > {};

    template < typename CallBacks >
    struct radio_with_a_void_cancel : dummy_radio< CallBacks >
    {
        void cancel_radio_event() {}
    };

    /*
     * start_advertising_event() cannot refuse, so a bool would report a broken caller as a
     * runtime condition; the concept asks for void and so rejects one that answers.
     */
    template < typename CallBacks >
    struct radio_whose_start_advertising_answers : dummy_radio< CallBacks >
    {
        bool start_advertising_event( std::uint32_t, const write_buffer&, const write_buffer&, const read_buffer& ) { return true; }
    };

    template < typename CallBacks >
    struct radio_without_a_radio_lock : dummy_radio< CallBacks >
    {
        using radio_lock_guard = void;
    };

    template < typename CallBacks >
    struct radio_without_a_link_layer_lock : dummy_radio< CallBacks >
    {
        using link_layer_lock_guard = void;
    };

    template < typename CallBacks >
    struct radio_with_a_hardware_acceptance_filter : dummy_radio< CallBacks >, model_acceptance_filter
    {
        static constexpr std::size_t radio_maximum_acceptance_filter_entries = 4;
    };

    template < typename CallBacks >
    struct radio_claiming_a_hardware_acceptance_filter_without_it : dummy_radio< CallBacks >
    {
        static constexpr std::size_t radio_maximum_acceptance_filter_entries = 4;
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
    static_assert( !scheduled_radio_callbacks< callbacks_without_the_connection_callbacks > );
    static_assert( !scheduled_radio_callbacks< callbacks_returning_the_buffer_by_value > );
    static_assert( !scheduled_radio_callbacks< callbacks_with_an_incomplete_buffer > );

    static_assert( software_acceptance_filter< model_callbacks > );
    static_assert( !software_acceptance_filter< callbacks_without_the_acceptance_filter > );

    static_assert( hardware_acceptance_filter< model_acceptance_filter > );
    static_assert( !hardware_acceptance_filter< acceptance_filter_without_clear > );

    static_assert( scheduled_radio_pdu_buffer< connection_callbacks::pdu_buffer > );

    static_assert( lesc_pairing_toolbox< dummy_toolbox > );
    static_assert( !lesc_pairing_toolbox< dummy_radio_base< model_callbacks > > );

    static_assert( scheduled_radio_features< dummy_features > );
    static_assert( !scheduled_radio_features< dummy_toolbox > );

    static_assert( scheduled_radio< dummy_radio, model_callbacks > );
    static_assert( scheduled_radio< radio_without_lesc_pairing, model_callbacks > );
    static_assert( !scheduled_radio< radio_claiming_lesc_pairing_without_the_toolbox, model_callbacks > );
    static_assert( !scheduled_radio< radio_with_a_void_cancel, model_callbacks > );
    static_assert( !scheduled_radio< radio_whose_start_advertising_answers, model_callbacks > );
    static_assert( !scheduled_radio< radio_without_a_radio_lock, model_callbacks > );
    static_assert( !scheduled_radio< radio_without_a_link_layer_lock, model_callbacks > );
    static_assert( !scheduled_radio< dummy_radio, callbacks_without_the_timer > );
    static_assert( scheduled_radio< radio_with_a_hardware_acceptance_filter, model_callbacks > );
    static_assert( !scheduled_radio< radio_claiming_a_hardware_acceptance_filter_without_it, model_callbacks > );
    // a radio that filters in hardware does not need the callback; one without it does
    static_assert( scheduled_radio< radio_with_a_hardware_acceptance_filter, callbacks_without_the_acceptance_filter > );
    static_assert( !scheduled_radio< dummy_radio, callbacks_without_the_acceptance_filter > );

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
    BOOST_CHECK( ( scheduled_radio< dummy_radio, model_callbacks > ) );
}
