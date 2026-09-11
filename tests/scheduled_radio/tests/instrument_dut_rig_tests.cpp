#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/dummy_port.hpp"
#include "host/dummy_radio.hpp"
#include "host/dut_functions.hpp"
#include "host/proxy.hpp"
#include "instrument/dut_rig.hpp"
#include "link/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace bluetoe::test_rig;

namespace {

    /*
     * The dummies of host/, instrumented: the radio counts what the rig calls on
     * it, the port keeps its instance and hands out its buffers, so that the test can see
     * what the rig did. Both keep the instance pointer a real port and radio keep for
     * their interrupt handlers, since the rig owns the objects.
     */
    template < typename CallBacks >
    class counting_radio : public dummy_radio< CallBacks >
    {
    public:
        counting_radio()
        {
            instance = this;
        }

        void run()
        {
            ++runs;
        }

        void wake_up()
        {
            ++wake_ups;
        }

        int runs     = 0;
        int wake_ups = 0;

        static inline counting_radio* instance = nullptr;
    };

    /*
     * A byte ring buffer behind virtual functions. The rig's buffer type exists only
     * inside the rig; this is how the test reaches the port's buffers without naming it.
     */
    class erased_buffer
    {
    public:
        virtual std::size_t free() const = 0;
        virtual void push( const std::uint8_t* data, std::size_t size ) = 0;
        virtual std::size_t available() const = 0;
        virtual void pop( std::uint8_t* data, std::size_t size ) = 0;

    protected:
        ~erased_buffer() = default;
    };

    template < typename Buffer >
    class erased_buffer_view : public erased_buffer
    {
    public:
        explicit erased_buffer_view( Buffer& buffer )
            : buffer_( buffer )
        {
        }

        std::size_t free() const override
        {
            return buffer_.free();
        }

        void push( const std::uint8_t* data, std::size_t size ) override
        {
            buffer_.push( data, size );
        }

        std::size_t available() const override
        {
            return buffer_.available();
        }

        void pop( std::uint8_t* data, std::size_t size ) override
        {
            buffer_.pop( data, size );
        }

    private:
        Buffer& buffer_;
    };

    /*
     * The part of the observed port that the test can name.
     */
    class observed_port_base
    {
    public:
        virtual erased_buffer& receive() = 0;
        virtual erased_buffer& transmit() = 0;

        // what the port does after it pushed received bytes
        virtual void wake() = 0;

        bool started       = false;
        int  transmissions = 0;

        static inline observed_port_base* instance = nullptr;

    protected:
        ~observed_port_base() = default;
    };

    template < typename Buffer, typename Wake >
    class observed_port : public observed_port_base
    {
    public:
        observed_port( Buffer& receive, Buffer& transmit, Wake& wake )
            : receive_( receive )
            , transmit_( transmit )
            , wake_( wake )
        {
            instance = this;
        }

        void start()
        {
            started = true;
        }

        void transmit_pending()
        {
            ++transmissions;
        }

        erased_buffer& receive() override
        {
            return receive_;
        }

        erased_buffer& transmit() override
        {
            return transmit_;
        }

        void wake() override
        {
            wake_.wake_up();
        }

    private:
        erased_buffer_view< Buffer >    receive_;
        erased_buffer_view< Buffer >    transmit_;
        Wake&                           wake_;
    };

    using rig_t = dut_rig< counting_radio, observed_port >;

    // the host's list is the rig's list, whatever the rig is instantiated with
    static_assert( dut_functions::size == rig_t::functions::size );

    /*
     * The host end of the observed port: a request goes as a frame into the rig's receive
     * buffer, the rig runs one iteration, and the response frame is taken out of its
     * transmit buffer.
     */
    struct rig_transport
    {
        rig_t                                   rig{ "counting radio on the host", "unit test build" };
        counting_radio< rig_t >&                radio = *counting_radio< rig_t >::instance;
        observed_port_base&                     port  = *observed_port_base::instance;
        frame_sender< erased_buffer >           sender{ port.receive() };
        frame_receiver< 256, erased_buffer >    receiver{ port.transmit() };

        std::vector< std::uint8_t > transact( std::span< const std::uint8_t > request )
        {
            if ( !sender.send( request ) )
                throw link_error( "request does not fit" );

            rig.run();

            if ( receiver.receive() != receive_result::frame )
                throw link_error( "no response" );

            return { receiver.payload().begin(), receiver.payload().end() };
        }
    };

    struct fixture
    {
        rig_transport                       transport;
        proxy< rig_t::functions, rig_transport > remote{ transport };
    };

    std::string_view as_text( const bytes< rig_t::name_size >& name )
    {
        return { reinterpret_cast< const char* >( name.data.data() ), name.size };
    }
}

BOOST_FIXTURE_TEST_CASE( the_port_is_started_on_construction, fixture )
{
    BOOST_CHECK( transport.port.started );
}

BOOST_FIXTURE_TEST_CASE( the_port_wakes_the_radio, fixture )
{
    transport.port.wake();

    BOOST_CHECK_EQUAL( transport.radio.wake_ups, 1 );
}

BOOST_FIXTURE_TEST_CASE( every_iteration_lets_the_radio_run, fixture )
{
    transport.rig.run();
    transport.rig.run();

    BOOST_CHECK_EQUAL( transport.radio.runs, 2 );
    BOOST_CHECK_EQUAL( transport.port.transmissions, 0 );
}

BOOST_FIXTURE_TEST_CASE( the_protocol_version_is_reported, fixture )
{
    BOOST_CHECK_EQUAL( remote.call< &rig_t::protocol_version >(), dut_protocol_version );
}

BOOST_FIXTURE_TEST_CASE( the_names_are_reported_as_given, fixture )
{
    BOOST_CHECK_EQUAL( as_text( remote.call< &rig_t::implementation_name >() ), "counting radio on the host" );
    BOOST_CHECK_EQUAL( as_text( remote.call< &rig_t::build_identifier >() ), "unit test build" );
}

BOOST_AUTO_TEST_CASE( a_name_longer_than_the_wire_type_is_truncated )
{
    const std::string long_name( 40, 'x' );
    rig_t             rig( long_name, "" );

    BOOST_CHECK_EQUAL( as_text( rig.implementation_name() ), std::string( rig_t::name_size, 'x' ) );
    BOOST_CHECK_EQUAL( as_text( rig.build_identifier() ), "" );
}

BOOST_FIXTURE_TEST_CASE( the_token_is_zero_after_start, fixture )
{
    // the proxy expects zero until told otherwise
    BOOST_CHECK_NO_THROW( remote.call< &rig_t::protocol_version >() );
}

BOOST_FIXTURE_TEST_CASE( set_session_token_answers_with_the_previous_token, fixture )
{
    BOOST_CHECK_NO_THROW( remote.call< &rig_t::set_session_token >( 0x12345678 ) );

    BOOST_CHECK_THROW( remote.call< &rig_t::protocol_version >(), instrument_restarted );

    remote.expect_token( 0x12345678 );
    BOOST_CHECK_NO_THROW( remote.call< &rig_t::protocol_version >() );
}

BOOST_FIXTURE_TEST_CASE( no_program_is_never_finished, fixture )
{
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( the_properties_are_those_of_the_radio, fixture )
{
    const auto properties = remote.call< &rig_t::properties >();

    BOOST_CHECK_EQUAL( properties.hardware_supports_encryption, true );
    BOOST_CHECK_EQUAL( properties.hardware_supports_lesc_pairing, true );
    BOOST_CHECK_EQUAL( properties.hardware_supports_legacy_pairing, true );
    BOOST_CHECK_EQUAL( properties.hardware_supports_2mbit, true );
    BOOST_CHECK_EQUAL( properties.hardware_supports_synchronized_user_timer, true );
    BOOST_CHECK_EQUAL( properties.radio_max_supported_payload_length, 251u );
    BOOST_CHECK_EQUAL( properties.sleep_time_accuracy_ppm, 250u );
}

BOOST_FIXTURE_TEST_CASE( a_response_that_does_not_fit_waits_for_room, fixture )
{
    std::vector< std::uint8_t > junk( transport.port.transmit().free(), 0xee );
    transport.port.transmit().push( junk.data(), junk.size() );

    const std::uint8_t request[] = { 0x00 };
    BOOST_REQUIRE( transport.sender.send( request ) );

    transport.rig.run();
    BOOST_CHECK_EQUAL( transport.port.transmissions, 0 );

    transport.port.transmit().pop( junk.data(), junk.size() );

    transport.rig.run();
    BOOST_CHECK_EQUAL( transport.port.transmissions, 1 );
    BOOST_CHECK( transport.receiver.receive() == receive_result::frame );
    BOOST_TEST( transport.receiver.payload() == std::vector< std::uint8_t >( { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( a_corrupt_frame_is_dropped_without_an_answer, fixture )
{
    // length 1, opcode 0, wrong CRC
    const std::uint8_t corrupt[] = { 0x01, 0x00, 0x00, 0xff, 0xff };
    transport.port.receive().push( corrupt, sizeof( corrupt ) );

    transport.rig.run();

    BOOST_CHECK_EQUAL( transport.port.transmissions, 0 );
    BOOST_CHECK( transport.receiver.receive() == receive_result::incomplete );

    BOOST_CHECK_EQUAL( remote.call< &rig_t::protocol_version >(), dut_protocol_version );
}
