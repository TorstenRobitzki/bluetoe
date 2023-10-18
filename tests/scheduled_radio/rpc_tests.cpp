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
};

bool remote_bool() {
    return false;
}

static const auto remote_functions = rpc::function_set<
    &virtual_remote_class::virtual_void,
    &remote_bool,
    &remote_prototype::prototype_f
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

BOOST_AUTO_TEST_CASE( call_first_function )
{
    stream io;
    static_assert( std::is_same< decltype( remote_functions.call< &virtual_remote_class::virtual_void >( io ) ), void >::value );

    remote_functions.call< &virtual_remote_class::virtual_void >( io );
    BOOST_TEST( io.data_written() == std::vector< std::uint8_t >({ 0x01 }) );
}

BOOST_AUTO_TEST_CASE( call_second_function )
{
    stream io;
    static_assert( std::is_same< decltype( remote_functions.call< &remote_bool >( io ) ), bool >::value );

    const auto rc = remote_functions.call< &remote_bool >( io );

    BOOST_TEST( io.data_written() == std::vector< std::uint8_t >({ 0x02 }) );
    BOOST_TEST( rc == false );
}

BOOST_AUTO_TEST_CASE( call_function_from_prototype )
{
    stream io;
    static_assert( std::is_same< decltype( remote_functions.call< &remote_prototype::prototype_f >( io ) ), std::uint32_t >::value );

    io.incomming_data( { 0x00, 0x01, 0x02, 0x03, 0x04 } );
    const auto rc = remote_functions.call< &remote_prototype::prototype_f >( io );

    BOOST_TEST( io.data_written() == std::vector< std::uint8_t >({ 0x03 }) );
    BOOST_TEST( rc == 0x04030201 );
}
