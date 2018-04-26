#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/encryption.hpp>
#include <bluetoe/server.hpp>

using service_uuid      = bluetoe::service_uuid16< 0x1234 >;
using service_uuid_a    = service_uuid;
using service_uuid_b    = bluetoe::service_uuid16< 0x1235 >;
using service_uuid_c    = bluetoe::service_uuid16< 0x1236 >;

using char_uuid         = bluetoe::characteristic_uuid16< 0x1234 >;
using char_uuid_1       = char_uuid;
using char_uuid_2       = bluetoe::characteristic_uuid16< 0x1235 >;
using char_uuid_3       = bluetoe::characteristic_uuid16< 0x1236 >;

std::int32_t dummy_val;

using char_1 = bluetoe::characteristic<
    bluetoe::bind_characteristic_value< decltype( dummy_val ), &dummy_val >,
    char_uuid
>;

BOOST_AUTO_TEST_CASE( by_default_no_encryption_is_required )
{
    using server = bluetoe::server<
        bluetoe::service<
            service_uuid,
            char_1
        >
    >;

    BOOST_CHECK( !bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( encryption_as_default_on_server_level )
{
    using server = bluetoe::server<
        bluetoe::requires_encryption,
        bluetoe::service<
            service_uuid,
            char_1
        >
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( no_encryption_as_default_on_server_level )
{
    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        bluetoe::service<
            service_uuid,
            char_1
        >
    >;

    BOOST_CHECK( !bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( a_service_requires_encryption )
{
    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        bluetoe::service<
            bluetoe::requires_encryption,
            service_uuid,
            char_1
        >
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( all_services_requires_encryption )
{
    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        bluetoe::service<
            bluetoe::requires_encryption,
            service_uuid_a,
            char_1
        >,
        bluetoe::service<
            bluetoe::requires_encryption,
            service_uuid_b,
            char_1
        >
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( some_services_requires_encryption )
{
    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        bluetoe::service<
            service_uuid_a,
            char_1
        >,
        bluetoe::service<
            bluetoe::requires_encryption,
            service_uuid_b,
            char_1
        >
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( some_services_requires_encryption_some_not )
{
    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        bluetoe::service<
            bluetoe::no_encryption_required,
            service_uuid_a,
            char_1
        >,
        bluetoe::service<
            bluetoe::requires_encryption,
            service_uuid_b,
            char_1
        >
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( default_requires_but_services_do_not )
{
    using server = bluetoe::server<
        bluetoe::requires_encryption,
        bluetoe::service<
            bluetoe::no_encryption_required,
            service_uuid_a,
            char_1
        >,
        bluetoe::service<
            bluetoe::no_encryption_required,
            service_uuid_b,
            char_1
        >
    >;

    BOOST_CHECK( !bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( one_services_requires_encryption )
{
    using server = bluetoe::server<
        bluetoe::service<
            service_uuid_a,
            char_1
        >,
        bluetoe::service<
            bluetoe::requires_encryption,
            service_uuid_b,
            char_1
        >
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( no_encryption_as_default_but_one_char_requires_encryption )
{
    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        bluetoe::service<
            service_uuid_a,
            char_1
        >,
        bluetoe::service<
            service_uuid_b,
            bluetoe::characteristic<
                bluetoe::bind_characteristic_value< decltype( dummy_val ), &dummy_val >,
                char_uuid,
                bluetoe::requires_encryption
            >
        >
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
}

BOOST_AUTO_TEST_CASE( service_requires_encryption_by_default_but_characteristic )
{
    using server = bluetoe::server<
        bluetoe::service<
            service_uuid_a,
            char_1
        >,
        bluetoe::service<
            bluetoe::requires_encryption,
            service_uuid_b,
            bluetoe::characteristic<
                bluetoe::bind_characteristic_value< decltype( dummy_val ), &dummy_val >,
                char_uuid,
                bluetoe::no_encryption_required
            >
        >
    >;

    BOOST_CHECK( !bluetoe::details::requires_encryption_support_t< server >::value );
}
