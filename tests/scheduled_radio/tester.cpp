#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include "rpc_declaration.hpp"
#include "nrf_uart_stream.hpp"
#include "serialize.hpp"
#include "radio.hpp"
#include "radio_callbacks.hpp"

auto protocol = rpc::protocol(
    tester_calling_iut_rpc_t(), iut_calling_tester_rpc_t() );

nrf_uart_stream< 28, 29, 30, 31 > io;

/*
 * We are not testing, that artifacts are declared in the correct namespace
 */
using bluetoe::link_layer::delta_time;
using bluetoe::link_layer::write_buffer;
using bluetoe::link_layer::read_buffer;
using namespace bluetoe::link_layer::details::phy_ll_encoding;

/**
 * @test using schedule_advertising_event(), make sure, that advertising comes
 *       at the right channel with the correct data and at the correct time.
 *
 * Asking for two advertisments to be able to use the first advertisment as reference
 */
TEST_CASE( "advertising" )
{
    const bluetoe::link_layer::random_device_address local_address({
        0x28, 0xb2, 0xa8, 0xee, 0xcb, 0x24
    });
    const std::uint32_t access_address = 0x12345678;
    const std::uint32_t crc_init       = 0x01020304;
    const std::uint8_t  adv_data[]     = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E };
    const std::uint8_t  adv_response_data[]     = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E };

    const write_buffer adv_data_buffer( adv_data );
    const write_buffer adv_response_data_buffer( adv_response_data );
    const read_buffer  receive_buffer = { nullptr, 0 };
    const delta_time   receive_timeout = delta_time::seconds( 5 );

    dut_callbacks callbacks;
    protocol.register_implementation< bluetoe::link_layer::example_callbacks >( callbacks );

    SECTION( "resonable timeing, correct data" )
    {
        const auto test_time = delta_time::seconds( 1 );
        const auto now = protocol.call< &scheduled_radio::time_now >( io );
        const auto t1  = now + delta_time::seconds( 1 );
        const auto t2  = t1  + test_time;

        const std::uint32_t accuracy_ppm = protocol.call< &scheduled_radio::dut_time_accuracy_ppm >( io );

        protocol.call< &scheduled_radio::set_local_address >( io, local_address );
        protocol.call< &scheduled_radio::set_access_address_and_crc_init >( io, access_address, crc_init );
        protocol.call< &scheduled_radio::set_phy >( io, le_1m_phy, le_1m_phy );

        SECTION( "Channel 37" )
        {
            protocol.call< &scheduled_radio::schedule_advertising_event >( io,
                37u, t1, adv_data_buffer, adv_response_data_buffer, receive_buffer );

            auto actual_t1 = radio::receive( 37u, access_address, le_1m_phy, receive_timeout );
            REQUIRE( actual_t1 );

            protocol.call< &scheduled_radio::schedule_advertising_event >( io,
                37u, t2, adv_data_buffer, adv_response_data_buffer, receive_buffer );

            auto actual_t2 = radio::receive( 37u, access_address, le_1m_phy, receive_timeout );
            REQUIRE( actual_t2 );

            const auto distance = *actual_t2 - *actual_t1;
            CHECK( distance >= test_time - test_time.ppm( accuracy_ppm ) );
            CHECK( distance <= test_time + test_time.ppm( accuracy_ppm ) );
        }

        SECTION( "Channel 38" )
        {
            protocol.call< &scheduled_radio::schedule_advertising_event >( io,
                38u, t1, adv_data_buffer, adv_response_data_buffer, receive_buffer );

            auto actual_t1 = radio::receive( 38u, access_address, le_1m_phy, receive_timeout );
            REQUIRE( actual_t1 );

            protocol.call< &scheduled_radio::schedule_advertising_event >( io,
                38u, t2, adv_data_buffer, adv_response_data_buffer, receive_buffer );

            auto actual_t2 = radio::receive( 38u, access_address, le_1m_phy, receive_timeout );
            REQUIRE( actual_t2 );

            const auto distance = *actual_t2 - *actual_t1;
            CHECK( distance >= test_time - test_time.ppm( accuracy_ppm ) );
            CHECK( distance <= test_time + test_time.ppm( accuracy_ppm ) );
        }

        SECTION( "Channel 39" )
        {
            protocol.call< &scheduled_radio::schedule_advertising_event >( io,
                39u, t1, adv_data_buffer, adv_response_data_buffer, receive_buffer );

            auto actual_t1 = radio::receive( 39u, access_address, le_1m_phy, receive_timeout );
            REQUIRE( actual_t1 );

            protocol.call< &scheduled_radio::schedule_advertising_event >( io,
                39u, t2, adv_data_buffer, adv_response_data_buffer, receive_buffer );

            auto actual_t2 = radio::receive( 39u, access_address, le_1m_phy, receive_timeout );
            REQUIRE( actual_t2 );

            const auto distance = *actual_t2 - *actual_t1;
            CHECK( distance >= test_time - test_time.ppm( accuracy_ppm ) );
            CHECK( distance <= test_time + test_time.ppm( accuracy_ppm ) );
        }
    }
}

int main()
{
    Catch::Session session;
    volatile int rc = session.run();

    for ( ;; )
        ;
}