#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "rpc.hpp"

#include <vector>
#include <type_traits>

struct virtual_remote_class {
    virtual ~virtual_remote_class() = default;

    virtual void virtual_void() = 0;
};

struct remote_prototype {
    std::uint32_t prototype_f();
    bool prototype_with_args( std::uint8_t p1, std::uint16_t p2 );
};

bool remote_bool();

static const auto remote_functions = rpc::remote_protocol<
    &virtual_remote_class::virtual_void,
    &remote_bool,
    &remote_prototype::prototype_f,
    &remote_prototype::prototype_with_args
>();

class stream
{
public:

    void put( std::uint8_t p )
    {
        written_.push_back(p);
    }

    void put( const std::uint8_t* p, std::size_t len )
    {
        written_.insert( written_.end(), p, p + len );
    }

    std::uint8_t get()
    {
        if ( read_.empty() )
            return 0;

        const std::uint8_t result = read_.front();
        read_.erase( read_.begin() );

        return result;
    }

    void get( std::uint8_t* p, std::size_t len )
    {
        for ( ; len; ++p, --len )
            *p = get();
    }

    std::vector< std::uint8_t > data_written()
    {
        std::vector< std::uint8_t > result;
        result.swap( written_ );

        return result;
    }

    void incomming_data( std::initializer_list< std::uint8_t > data )
    {
        read_.insert( read_.end(), data.begin(), data.end() );
    }

    bool has_incomming_data() const
    {
        return !read_.empty();
    }

private:
    std::vector< std::uint8_t > written_;
    std::vector< std::uint8_t > read_;
};

const auto pp = boost::test_tools::per_element();

std::vector< std::uint8_t > v( std::initializer_list< std::uint8_t > l )
{
    return l;
}

BOOST_AUTO_TEST_CASE( call_first_function )
{
    stream io;
    static_assert( std::is_same< decltype( remote_functions.call< &virtual_remote_class::virtual_void >( io ) ), void >::value );

    remote_functions.call< &virtual_remote_class::virtual_void >( io );
    BOOST_TEST( io.data_written() == v({ 0x01 }), pp );
}

BOOST_AUTO_TEST_CASE( call_second_function )
{
    stream io;
    static_assert( std::is_same< decltype( remote_functions.call< &remote_bool >( io ) ), bool >::value );

    const auto rc = remote_functions.call< &remote_bool >( io );

    BOOST_TEST( io.data_written() == v({ 0x02 }), pp );
    BOOST_TEST( rc == false );
}

BOOST_AUTO_TEST_CASE( call_function_from_prototype )
{
    stream io;
    static_assert( std::is_same< decltype( remote_functions.call< &remote_prototype::prototype_f >( io ) ), std::uint32_t >::value );

    io.incomming_data( { 0x00, 0x01, 0x02, 0x03, 0x04 } );
    const auto rc = remote_functions.call< &remote_prototype::prototype_f >( io );

    BOOST_TEST( io.data_written() == v({ 0x03 }), pp );
    BOOST_TEST( rc == 0x04030201 );
}

BOOST_AUTO_TEST_CASE( call_function_from_prototype_with_args )
{
    stream io;
    static_assert( std::is_same< decltype( remote_functions.call< &remote_prototype::prototype_with_args >( io ) ), bool >::value );

    io.incomming_data( { 0x00, 0x01 } );
    const auto rc = remote_functions.call< &remote_prototype::prototype_with_args >( io, 0x42, 0x0102 );

    BOOST_TEST( io.data_written() == v({ 0x04, 0x42, 0x02, 0x01 }), pp );
    BOOST_TEST( rc == true );
}

bool void1_called = false;
bool void2_called = false;

void f_void1()
{
    void1_called = true;
}

void f_void2()
{
    void2_called = true;
}

bool f_bool_called;
std::uint8_t f_bool_r;

bool f_bool( std::uint8_t r )
{
    f_bool_called = true;
    f_bool_r = r;

    return r & 0x01;
}

bool f_uint32_called;
std::uint16_t f_uint32_u;
bool f_uint32_b;

std::uint32_t f_uint32( std::uint16_t u, bool b )
{
    f_uint32_called = true;
    f_uint32_u = u;
    f_uint32_b = b;

    return u << 8;
}

struct reset_call_markers
{
    reset_call_markers()
    {
        void1_called = false;
        void2_called = false;
        f_bool_called = false;
        f_bool_r = ~0;
        f_uint32_called = false;
        f_uint32_u = ~0;
        f_uint32_b = false;
    }
};

static const auto local_void_functions = rpc::local_protocol<
    &f_void1,
    &f_void2,
    &f_bool,
    &f_uint32
>();

BOOST_FIXTURE_TEST_CASE( deserialize_call_to_void_func, reset_call_markers )
{
    stream io;
    io.incomming_data( { 0x01 } );
    local_void_functions.deserialize_call( io );

    BOOST_TEST( void1_called );
    BOOST_TEST( !void2_called );
    BOOST_TEST( io.data_written().empty() );
}

BOOST_FIXTURE_TEST_CASE( deserialize_call_to_void_func2, reset_call_markers )
{
    stream io;
    io.incomming_data( { 0x02 } );
    local_void_functions.deserialize_call( io );

    BOOST_TEST( !void1_called );
    BOOST_TEST( void2_called );
    BOOST_TEST( io.data_written().empty() );
}

BOOST_FIXTURE_TEST_CASE( deserialize_call_to_f_bool, reset_call_markers )
{
    stream io;
    io.incomming_data( { 0x03, 0x42 } );
    local_void_functions.deserialize_call( io );

    BOOST_TEST( f_bool_called );
    BOOST_TEST( f_bool_r == 0x42 );
    BOOST_TEST( io.data_written() == v({ 0x00, 0x00 }), pp );
}

BOOST_FIXTURE_TEST_CASE( deserialize_call_to_f_uint32, reset_call_markers )
{
    stream io;
    io.incomming_data( { 0x04, 0xaa, 0xbb, 0x00 } );
    local_void_functions.deserialize_call( io );

    BOOST_TEST( f_uint32_called );
    BOOST_TEST( f_uint32_u == 0xbbaa );
    BOOST_TEST( f_uint32_b == false );
    BOOST_TEST( io.data_written() == v({ 0x00, 0x00, 0xaa, 0xbb, 0x00 }), pp );
}

struct local_object_A
{
    void f1()
    {
        instance_used = this;
        f1_called = true;
    }

    void f2( std::uint16_t a )
    {
        instance_used = this;
        f2_called = true;
        f2_a = a;
    }

    std::uint32_t f3()
    {
        instance_used = this;
        f3_called = true;
        return f3_result;
    }

    local_object_A* instance_used = nullptr;
    bool f1_called = false;
    bool f2_called = false;
    std::uint16_t f2_a = 0;
    bool f3_called = false;
    std::uint32_t f3_result = 0x01020304;
};

struct local_object_B
{
    bool f1( std::tuple< std::uint8_t, std::uint32_t > t )
    {
        instance_used = this;
        f1_called = true;
        f1_t = t;

        return f1_result;
    }

    local_object_B* instance_used = nullptr;
    bool f1_called = false;
    bool f1_result = true;
    std::tuple< std::uint8_t, std::uint32_t > f1_t = { 0, 0 };
};

BOOST_AUTO_TEST_CASE( deserialize_call_to_member_functions )
{
    auto local_member_functions = rpc::local_protocol<
        &local_object_A::f1,
        &local_object_A::f2,
        &local_object_A::f3,
        &local_object_B::f1
    >();

    local_object_A local_a;
    local_object_B local_b;

    local_member_functions.register_implementation( local_a );
    local_member_functions.register_implementation( local_b );

    stream io;
    io.incomming_data( { 0x01 } );
    io.incomming_data( { 0x02, 0xaa, 0xbb } );
    io.incomming_data( { 0x03 } );
    io.incomming_data( { 0x04, 0xff, 0x11, 0x22, 0x33, 0x44 } );

    while ( io.has_incomming_data() )
        local_member_functions.deserialize_call( io );

    // functions called:
    BOOST_TEST( local_a.instance_used == &local_a );
    BOOST_TEST( local_b.instance_used == &local_b );

    BOOST_TEST( local_a.f1_called );

    BOOST_TEST( local_a.f2_called );
    BOOST_TEST( local_a.f2_a == 0xbbaa );

    BOOST_TEST( local_a.f3_called );

    BOOST_TEST( local_b.f1_called );
    BOOST_TEST( std::get< 0 >( local_b.f1_t ) == 0xff );
    BOOST_TEST( std::get< 1 >( local_b.f1_t ) == 0x44332211 );

    // return values:
    BOOST_TEST( io.data_written() == v(
        {
            0x00,
            0x04, 0x03, 0x02, 0x01,
            0x00,
            0x01
        }), pp );

}

const auto bidirectional_protocol = rpc::protocol(
    rpc::remote_protocol< &remote_bool >(),
    rpc::local_protocol< &f_void1, &f_uint32 >()
);

BOOST_AUTO_TEST_CASE( mixed_directions_remote_call )
{
    stream io;
    bidirectional_protocol.call< &remote_bool >( io );

    BOOST_TEST( io.data_written() == v( { 0x01 } ), pp );
}

BOOST_FIXTURE_TEST_CASE( mixed_directions_remote_call_with_incomming_calls, reset_call_markers )
{
    stream io;
    // call to f_uint32( 0xbbaa, true )
    io.incomming_data( { 0x02, 0xaa, 0xbb, 0x01 } );

    // call to remote, while there is a call to f_uint32 from the remote side
    // on the line already
    bidirectional_protocol.call< &remote_bool >( io );

    BOOST_TEST( f_uint32_called );
    BOOST_TEST( f_uint32_u == 0xbbaa );
    BOOST_TEST( f_uint32_b == true );

    // the call to remote_bool(), followed by the result of f_uint32
    BOOST_TEST( io.data_written() == v( { 0x01, 0x00, 0x00, 0xaa, 0xbb, 0x00 } ), pp );
}

void func_ref( const uint8_t& );

BOOST_AUTO_TEST_CASE( call_functions_with_reference_parameter_types )
{
    stream io;

    auto prot = rpc::remote_protocol< &func_ref >();
    prot.call< &func_ref >( io, uint8_t{ 42 } );
}