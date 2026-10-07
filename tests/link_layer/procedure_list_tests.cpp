#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/procedures.hpp>

#include "procedure_mocks.hpp"
#include "procedures_io.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace bll = bluetoe::link_layer;

namespace {

    class fixture_procedure
    {
    public:
        // a made up opcode
        static constexpr std::uint8_t opcode        = 0xF0;
        static constexpr std::uint8_t ctr_data_size = 4;

        template < class LinkLayer, class LinkData >
        static bll::details::procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            ++link.side_effect;

            return bll::details::procedure_result::handled();
        }

        struct state_type {};
        struct instant_state {};
    };

    /*
     * A made up request with a made up response: it needs room for its answer.
     */
    class request_fixture
    {
    public:
        static constexpr std::uint8_t opcode        = 0xF2;
        static constexpr std::uint8_t response      = 0xF3;
        static constexpr std::uint8_t ctr_data_size = 0;

        template < class LinkLayer, class LinkData >
        static bll::details::procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            const auto write = link.buffers.allocate_ll_transmit_buffer( 1 );

            if ( write.size == 0 )
                return bll::details::procedure_result::stalled();

            bluetoe::link_layer::fill< typename decltype( link.buffers )::layout >( write, { 0x03, 1, response } );
            link.buffers.commit_ll_transmit_buffer( write );

            return bll::details::procedure_result::handled();
        }

        struct state_type {};
        struct instant_state {};
    };

    /*
     * A made up indication with an instant, like LL_CONNECTION_UPDATE_IND: CtrData is a value
     * and the instant. At the instant, the procedure records its opcode and the value in the
     * link data. Two opcodes make two different procedures.
     */
    template < std::uint8_t Opcode >
    class instant_fixture : public bll::details::procedure_with_instant
    {
    public:
        static constexpr std::uint8_t opcode        = Opcode;
        static constexpr std::uint8_t ctr_data_size = 3;

        struct instant_state
        {
            std::uint8_t value;
        };

        template < class LinkLayer, class LinkData >
        static bll::details::procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > pdu )
        {
            return apply_and_check_instant( instant_state{ pdu[ 1 ] }, link, bluetoe::details::read_16bit( &pdu[ 2 ] ) );
        }

        template < class LinkLayer, class LinkData >
        static bool connection_event( LinkLayer& /*link_layer*/, LinkData& link, bll::details::procedure_result& /*result*/ )
        {
            const auto state = std::get_if< instant_state >( &link.procedures_instant_state );

            if ( !state )
                return false;

            link.applied.push_back( { opcode, state->value } );

            return true;
        }

        struct state_type {};
    };

    struct procedure_with_state
    {
        static constexpr std::uint8_t opcode        = 0xF6;
        static constexpr std::uint8_t ctr_data_size = 0;
        static constexpr int state_init             = 42;

        template < class LinkLayer, class LinkData >
        static bluetoe::link_layer::details::procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            ++static_cast< state_type& >( link ).state;
            return bluetoe::link_layer::details::procedure_result::handled();
        }

        struct state_type { int state = state_init; };
        struct instant_state {};
    };

    using instant_fixture_a = instant_fixture< 0xF4 >;
    using instant_fixture_b = instant_fixture< 0xF5 >;

    // the PDU of an instant_fixture
    std::vector< std::uint8_t > instant_pdu( std::uint8_t opcode, std::uint8_t value, std::uint16_t instant )
    {
        return control_pdu( {
            opcode,
            value,
            static_cast< std::uint8_t >( instant ), static_cast< std::uint8_t >( instant >> 8 ) } );
    }

    using mixed_procedures = bll::details::procedure_list< fixture_procedure, request_fixture, instant_fixture_a, instant_fixture_b >;

    struct mixed_link_data_mock : link_data_mock< mixed_procedures >
    {
        int side_effect = 0;

        // opcode and value of each instant_fixture, in the order applied
        std::vector< std::pair< std::uint8_t, std::uint8_t > > applied;
    };

    using applied_list = std::vector< std::pair< std::uint8_t, std::uint8_t > >;

    // Core Vol 1, Part F: error code 0x23, LL Procedure Collision
    constexpr std::uint8_t ll_procedure_collision = 0x23;

    // Core Vol 1, Part F: error code 0x2A, Different Transaction Collision
    constexpr std::uint8_t different_transaction_collision = 0x2A;

    using no_procedures = bll::details::procedure_list<>;

    using single_procedure = bll::details::procedure_list< fixture_procedure >;

    struct single_procedure_link_data_mock : link_data_mock< single_procedure >
    {
        int side_effect = 0;
    };

    const auto ping_req = control_pdu( {
        0x12                // LL_PING_REQ
    } );
}

BOOST_AUTO_TEST_CASE( an_empty_list_answers_any_request_as_unknown )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,               // LL_UNKNOWN_RSP
        0x12                // UnknownType: LL_PING_REQ
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

BOOST_AUTO_TEST_CASE( an_unknown_request_stalls_without_room_for_the_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    link.buffers.room_for_an_answer = false;

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * A stalled PDU stays in the receive buffer, and the link layer passes it again at the next
 * connection event; once there is room, it is answered, once.
 */
BOOST_AUTO_TEST_CASE( a_stalled_request_is_answered_when_there_is_room )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    link.buffers.room_for_an_answer = false;

    const auto stalled = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( stalled == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.buffers.transmitted.empty() );

    link.buffers.room_for_an_answer = true;

    const auto handled = no_procedures::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( handled == bll::details::procedure_result::handled() );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,               // LL_UNKNOWN_RSP
        0x12                // UnknownType: LL_PING_REQ
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

/*
 * An LL control PDU shall not have a length of 0. There is no opcode to report back, and the
 * connection has to keep being served, so such a PDU is ignored.
 */
BOOST_AUTO_TEST_CASE( a_control_pdu_without_an_opcode_is_ignored )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    const auto no_opcode = control_pdu( {} );

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( no_opcode ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * Ignoring needs no answer, so a PDU without an opcode never stalls the PDUs behind it.
 */
BOOST_AUTO_TEST_CASE( a_control_pdu_without_an_opcode_is_ignored_without_room_for_an_answer )
{
    link_layer_mock                 link_layer;
    link_data_mock< no_procedures > link;

    link.buffers.room_for_an_answer = false;

    const auto no_opcode = control_pdu( {} );

    const auto result = no_procedures::handle_control_pdu( link_layer, link, payload( no_opcode ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( a_known_opcode_is_passed_to_its_procedure )
{
    link_layer_mock                  link_layer;
    single_procedure_link_data_mock  link;

    const auto request = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03, 0x04 } );

    const auto result = single_procedure::handle_control_pdu( link_layer, link, payload( request ) );

    BOOST_TEST( link.side_effect == 1 );
    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( known_opcode_but_request_too_long )
{
    link_layer_mock                  link_layer;
    single_procedure_link_data_mock  link;

    // the correct size would be one opcode and 4 bytes of CtrData
    const auto invalid_size = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03, 0x04, 0x05 } );

    const auto result = single_procedure::handle_control_pdu( link_layer, link, payload( invalid_size ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.side_effect == 0 );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,                       // LL_UNKNOWN_RSP
        fixture_procedure::opcode   // UnknownType: fixture_procedure::opcode
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

BOOST_AUTO_TEST_CASE( known_opcode_but_request_too_small )
{
    link_layer_mock                  link_layer;
    single_procedure_link_data_mock  link;

    // the correct size would be one opcode and 4 bytes of CtrData
    const auto invalid_size = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03 } );

    const auto result = single_procedure::handle_control_pdu( link_layer, link, payload( invalid_size ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.side_effect == 0 );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto unknown_rsp = control_pdu( {
        0x07,                       // LL_UNKNOWN_RSP
        fixture_procedure::opcode   // UnknownType: fixture_procedure::opcode
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == unknown_rsp );
}

BOOST_AUTO_TEST_CASE( known_opcode_but_request_too_long_without_room_for_an_answer )
{
    link_layer_mock                  link_layer;
    single_procedure_link_data_mock  link;

    link.buffers.room_for_an_answer = false;

    const auto invalid_size = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03, 0x04, 0x05 } );

    const auto result = single_procedure::handle_control_pdu( link_layer, link, payload( invalid_size ) );

    BOOST_TEST( result == bll::details::procedure_result::stalled() );
    BOOST_TEST( link.side_effect == 0 );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

/*
 * Procedures next to each other
 *
 * Only one procedure with an instant can be pending; every other control PDU is handled
 * meanwhile, without holding back the instant.
 */
BOOST_AUTO_TEST_CASE( a_request_is_answered_while_an_instant_is_pending )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    link.connection_event_counter_value = 100;

    BOOST_TEST( mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_a::opcode, 1, 106 ) ) )
        == bll::details::procedure_result::handled() );

    const auto request = control_pdu( {
        request_fixture::opcode
    } );

    const auto result = mixed_procedures::handle_control_pdu( link_layer, link, payload( request ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.buffers.transmitted == std::vector({ control_pdu( {
        request_fixture::response
    } ) }));

    BOOST_TEST( link.applied.empty() );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link.applied == ( applied_list{ { instant_fixture_a::opcode, 1 } } ) );
}

BOOST_AUTO_TEST_CASE( an_indication_without_instant_is_handled_while_an_instant_is_pending )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    link.connection_event_counter_value = 100;

    mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_a::opcode, 1, 106 ) ) );

    const auto indication = control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03, 0x04 } );
    const auto result     = mixed_procedures::handle_control_pdu( link_layer, link, payload( indication ) );

    BOOST_TEST( result == bll::details::procedure_result::handled() );
    BOOST_TEST( link.side_effect == 1 );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link.applied == ( applied_list{ { instant_fixture_a::opcode, 1 } } ) );
}

// a request that waits for room in the transmit buffer does not hold back the instant
BOOST_AUTO_TEST_CASE( a_stalled_request_does_not_hold_back_the_instant )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    link.connection_event_counter_value = 100;
    link.buffers.room_for_an_answer = false;

    mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_a::opcode, 1, 106 ) ) );

    const auto request = control_pdu( { request_fixture::opcode } );
    BOOST_TEST( mixed_procedures::handle_control_pdu( link_layer, link, payload( request ) )
        == bll::details::procedure_result::stalled() );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link.applied == ( applied_list{ { instant_fixture_a::opcode, 1 } } ) );
}

// with two procedures with an instant in the list, only the one that set it is called
BOOST_AUTO_TEST_CASE( the_instant_goes_to_the_procedure_that_set_it )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    link.connection_event_counter_value = 100;

    mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_b::opcode, 2, 106 ) ) );

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link.applied == ( applied_list{ { instant_fixture_b::opcode, 2 } } ) );
}

BOOST_AUTO_TEST_CASE( without_a_pending_instant_no_procedure_is_called )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    run_connection_events( link_layer, link, 1000 );
    BOOST_TEST( link.applied.empty() );
}

BOOST_AUTO_TEST_CASE( a_new_instant_is_accepted_once_the_last_one_was_applied )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    link.connection_event_counter_value = 100;

    mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_a::opcode, 1, 106 ) ) );
    run_connection_events( link_layer, link, 106 );

    const auto result = mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_b::opcode, 2, 112 ) ) );
    BOOST_TEST( result == bll::details::procedure_result::handled() );

    run_connection_events( link_layer, link, 112 );
    BOOST_TEST( link.applied == ( applied_list{ { instant_fixture_a::opcode, 1 }, { instant_fixture_b::opcode, 2 } } ) );
}

/*
 * Collisions of procedures with an instant
 *
 * A central does not start a procedure with an instant while another one is pending, and no
 * test of the LL Test Suite sends one to a peripheral. An indication has no answer to reject
 * it with, and the peripheral can follow only one of them, so Bluetoe ends the connection:
 * with LL Procedure Collision for the same procedure, with Different Transaction Collision
 * for another one.
 */
BOOST_AUTO_TEST_CASE( the_same_procedure_while_its_instant_is_pending_disconnects )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    link.connection_event_counter_value = 100;

    mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_a::opcode, 1, 106 ) ) );

    const auto result = mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_a::opcode, 2, 110 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( ll_procedure_collision ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( another_procedure_with_an_instant_while_one_is_pending_disconnects )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    link.connection_event_counter_value = 100;

    mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_a::opcode, 1, 106 ) ) );

    const auto result = mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_b::opcode, 2, 110 ) ) );

    BOOST_TEST( result == bll::details::procedure_result::disconnect( different_transaction_collision ) );
    BOOST_TEST( link.buffers.transmitted.empty() );
}

BOOST_AUTO_TEST_CASE( after_a_link_got_reset_instant_gets_reset )
{
    link_layer_mock         link_layer;
    mixed_link_data_mock    link;

    link.connection_event_counter_value = 100;

    BOOST_TEST( mixed_procedures::handle_control_pdu( link_layer, link, payload( instant_pdu( instant_fixture_a::opcode, 1, 106 ) ) )
        == bll::details::procedure_result::handled() );

    mixed_procedures::connection_reset( link );

    link.connection_event_counter_value = 100;

    run_connection_events( link_layer, link, 106 );
    BOOST_TEST( link.applied.empty() );
}

using mixed_procedures = bll::details::procedure_list< fixture_procedure, request_fixture, instant_fixture_a, instant_fixture_b >;

BOOST_AUTO_TEST_CASE( after_a_link_got_reset_state_type_gets_reset )
{
    using list_t = bll::details::procedure_list< procedure_with_state >;
    link_layer_mock          link_layer;
    link_data_mock< list_t > link;

    link.connection_event_counter_value = 100;

    const auto rc = list_t::handle_control_pdu( link_layer, link, payload( control_pdu( { procedure_with_state::opcode } )  ) );

    BOOST_TEST( rc == bll::details::procedure_result::handled() );
    BOOST_TEST( static_cast< procedure_with_state::state_type& >( link ).state != procedure_with_state::state_init );

    list_t::connection_reset( link );
    BOOST_TEST( static_cast< procedure_with_state::state_type& >( link ).state == procedure_with_state::state_init );
}

/*
 * Procedures that consist of several parts
 *
 * A procedure of the specification with more than one received opcode, such as the PHY update
 * with LL_PHY_REQ and LL_PHY_UPDATE_IND, is a procedure_group<> of parts, one part per opcode.
 * The list handles the parts as if they were listed one by one. One part owns the state that
 * the parts share, and declares the feature bit of the procedure.
 */
namespace {

    // the owner: counts the PDUs of the group in the shared state
    struct group_owner_part
    {
        static constexpr std::uint8_t                           opcode        = 0xF6;
        static constexpr std::uint8_t                           ctr_data_size = 0;
        static constexpr bll::details::link_layer_feature       feature_flag  = bll::details::link_layer_feature::extended_reject_indication;

        struct state_type
        {
            int pdus = 0;
        };

        struct instant_state {};

        template < class LinkLayer, class LinkData >
        static bll::details::procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            state_type& state = link;
            ++state.pdus;

            return bll::details::procedure_result::handled();
        }
    };

    // the other part: has no state of its own, and counts in the owner's
    struct group_other_part
    {
        static constexpr std::uint8_t opcode        = 0xF7;
        static constexpr std::uint8_t ctr_data_size = 0;

        struct state_type {};
        struct instant_state {};

        template < class LinkLayer, class LinkData >
        static bll::details::procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            group_owner_part::state_type& state = link;
            ++state.pdus;

            return bll::details::procedure_result::handled();
        }
    };

    using fixture_group = bll::details::procedure_group< group_owner_part, group_other_part >;

    using group_only      = bll::details::procedure_list< fixture_group >;
    using group_and_more  = bll::details::procedure_list< fixture_procedure, fixture_group, request_fixture >;

    struct group_and_more_link_data_mock : link_data_mock< group_and_more >
    {
        int side_effect = 0;
    };

    int group_pdus( const auto& link )
    {
        return static_cast< const group_owner_part::state_type& >( link ).pdus;
    }

    const auto owner_pdu = control_pdu( { group_owner_part::opcode } );
    const auto other_pdu = control_pdu( { group_other_part::opcode } );
}

BOOST_AUTO_TEST_CASE( both_parts_of_a_group_are_found )
{
    link_layer_mock                 link_layer;
    link_data_mock< group_only >    link;

    BOOST_TEST( group_only::handle_control_pdu( link_layer, link, payload( owner_pdu ) ) == bll::details::procedure_result::handled() );
    BOOST_TEST( group_only::handle_control_pdu( link_layer, link, payload( other_pdu ) ) == bll::details::procedure_result::handled() );

    // neither was answered as unknown
    BOOST_TEST( link.buffers.transmitted.empty() );
    BOOST_TEST( group_pdus( link ) == 2 );
}

// a group next to single procedures: every opcode reaches its procedure, an unknown one is still unknown
BOOST_AUTO_TEST_CASE( a_group_and_single_procedures_share_a_list )
{
    link_layer_mock                 link_layer;
    group_and_more_link_data_mock   link;

    group_and_more::handle_control_pdu( link_layer, link, payload( control_pdu( { fixture_procedure::opcode, 0x01, 0x02, 0x03, 0x04 } ) ) );
    group_and_more::handle_control_pdu( link_layer, link, payload( other_pdu ) );
    group_and_more::handle_control_pdu( link_layer, link, payload( control_pdu( { request_fixture::opcode } ) ) );
    group_and_more::handle_control_pdu( link_layer, link, payload( ping_req ) );

    BOOST_TEST( link.side_effect == 1 );
    BOOST_TEST( group_pdus( link ) == 1 );
    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 2u );

    const auto response = control_pdu( {
        request_fixture::response
    } );

    const auto unknown_rsp = control_pdu( {
        0x07,               // LL_UNKNOWN_RSP
        0x12                // UnknownType: LL_PING_REQ
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == response );
    BOOST_TEST( link.buffers.transmitted[ 1 ] == unknown_rsp );
}

BOOST_AUTO_TEST_CASE( the_parts_of_a_group_share_the_state_of_the_owner )
{
    link_layer_mock                 link_layer;
    link_data_mock< group_only >    link;

    group_only::handle_control_pdu( link_layer, link, payload( other_pdu ) );
    group_only::handle_control_pdu( link_layer, link, payload( owner_pdu ) );
    group_only::handle_control_pdu( link_layer, link, payload( other_pdu ) );

    BOOST_TEST( group_pdus( link ) == 3 );
}

BOOST_AUTO_TEST_CASE( the_state_of_a_group_is_reset_with_the_link )
{
    link_layer_mock                 link_layer;
    link_data_mock< group_only >    link;

    group_only::handle_control_pdu( link_layer, link, payload( owner_pdu ) );
    group_only::connection_reset( link );

    BOOST_TEST( group_pdus( link ) == 0 );
}

// the feature bit a part declares is a feature of the list
BOOST_AUTO_TEST_CASE( the_feature_bit_of_a_group_is_a_feature_of_the_list )
{
    using features_and_group = bll::details::procedure_list< bll::details::feature_exchange_procedure, fixture_group >;

    link_layer_mock                         link_layer;
    link_data_mock< features_and_group >    link;

    const auto feature_req = control_pdu( {
        0x08,                                           // LL_FEATURE_REQ
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // FeatureSet: all features
    } );

    features_and_group::handle_control_pdu( link_layer, link, payload( feature_req ) );

    BOOST_REQUIRE_EQUAL( link.buffers.transmitted.size(), 1u );

    const auto feature_rsp = control_pdu( {
        0x09,                                           // LL_FEATURE_RSP
        0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // FeatureSet: bit 2, Extended Reject Indication
    } );

    BOOST_TEST( link.buffers.transmitted[ 0 ] == feature_rsp );
}
