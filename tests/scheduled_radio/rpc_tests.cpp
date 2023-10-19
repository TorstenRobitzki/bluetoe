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

bool remote_bool() {
    return false;
}

static const auto remote_functions = rpc::remote_protocol<
    &virtual_remote_class::virtual_void,
    &remote_bool,
    &remote_prototype::prototype_f,
    &remote_prototype::prototype_with_args
>();

class stream
{
public:

    void put(std::uint8_t p)
    {
        written_.push_back(p);
    }

    std::uint8_t get()
    {
        if ( read_.empty() )
            return 0;

        const std::uint8_t result = read_.front();
        read_.erase( read_.begin() );

        return result;
    }

    std::vector< std::uint8_t > data_written()
    {
        std::vector< std::uint8_t > result;
        result.swap( written_ );

        return result;
    }

    void incomming_data( std::initializer_list< std::uint8_t > data )
    {
        read_.insert( read_.begin(), data.begin(), data.end() );
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
