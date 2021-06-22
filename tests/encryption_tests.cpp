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
    using service = bluetoe::service<
        service_uuid,
        char_1
    >;

    using server = bluetoe::server< service >;

    BOOST_CHECK( !bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service, server >::value ));
}

BOOST_AUTO_TEST_CASE( encryption_as_default_on_server_level )
{
    using service = bluetoe::service<
        service_uuid,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::requires_encryption,
        service
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( bluetoe::details::characteristic_requires_encryption< char_1, service, server >::value ));
}

BOOST_AUTO_TEST_CASE( no_encryption_as_default_on_server_level )
{
    using service = bluetoe::service<
        service_uuid,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        service
    >;


    BOOST_CHECK( !bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service, server >::value ));
}

BOOST_AUTO_TEST_CASE( a_service_requires_encryption )
{
    using service = bluetoe::service<
        bluetoe::requires_encryption,
        service_uuid,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        service
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( bluetoe::details::characteristic_requires_encryption< char_1, service, server >::value ));
}

BOOST_AUTO_TEST_CASE( all_services_requires_encryption )
{
    using service_1 = bluetoe::service<
        bluetoe::requires_encryption,
        service_uuid_a,
        char_1
    >;

    using service_2 = bluetoe::service<
        bluetoe::requires_encryption,
        service_uuid_b,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        service_1,
        service_2
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( bluetoe::details::characteristic_requires_encryption< char_1, service_1, server >::value ));
    BOOST_CHECK(( bluetoe::details::characteristic_requires_encryption< char_1, service_2, server >::value ));
}

BOOST_AUTO_TEST_CASE( some_services_requires_encryption )
{
    using service_1 = bluetoe::service<
        service_uuid_a,
        char_1
    >;

    using service_2 = bluetoe::service<
        bluetoe::requires_encryption,
        service_uuid_b,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        service_1,
        service_2
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service_1, server >::value ));
    BOOST_CHECK(( bluetoe::details::characteristic_requires_encryption< char_1, service_2, server >::value ));
}

BOOST_AUTO_TEST_CASE( some_services_requires_encryption_some_not )
{
    using service_1 = bluetoe::service<
        bluetoe::no_encryption_required,
        service_uuid_a,
        char_1
    >;

    using service_2 = bluetoe::service<
        bluetoe::requires_encryption,
        service_uuid_b,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        service_1,
        service_2
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service_1, server >::value ));
    BOOST_CHECK(( bluetoe::details::characteristic_requires_encryption< char_1, service_2, server >::value ));
}

BOOST_AUTO_TEST_CASE( default_requires_but_services_do_not )
{
    using service_1 = bluetoe::service<
        bluetoe::no_encryption_required,
        service_uuid_a,
        char_1
    >;

    using service_2 = bluetoe::service<
        bluetoe::no_encryption_required,
        service_uuid_b,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::requires_encryption,
        service_1,
        service_2
    >;

    BOOST_CHECK( !bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service_1, server >::value ));
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service_2, server >::value ));
}

BOOST_AUTO_TEST_CASE( one_services_requires_encryption )
{
    using service_1 = bluetoe::service<
        service_uuid_a,
        char_1
    >;

    using service_2 = bluetoe::service<
        bluetoe::requires_encryption,
        service_uuid_b,
        char_1
    >;

    using server = bluetoe::server<
        service_1,
        service_2
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service_1, server >::value ));
    BOOST_CHECK(( bluetoe::details::characteristic_requires_encryption< char_1, service_2, server >::value ));
}

BOOST_AUTO_TEST_CASE( no_encryption_as_default_but_one_char_requires_encryption )
{
    using service_1 = bluetoe::service<
        service_uuid_a,
        char_1
    >;

    using char_req = bluetoe::characteristic<
        bluetoe::bind_characteristic_value< decltype( dummy_val ), &dummy_val >,
        char_uuid,
        bluetoe::requires_encryption
    >;

    using service_2 = bluetoe::service<
        service_uuid_b,
        char_req
    >;

    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        service_1,
        service_2
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service_1, server >::value ));
    BOOST_CHECK(( bluetoe::details::characteristic_requires_encryption< char_req, service_2, server >::value ));
}

BOOST_AUTO_TEST_CASE( service_requires_encryption_by_default_but_characteristic )
{
    using service_1 = bluetoe::service<
        service_uuid_a,
        char_1
    >;

    using char_no_enc = bluetoe::characteristic<
        bluetoe::bind_characteristic_value< decltype( dummy_val ), &dummy_val >,
        char_uuid,
        bluetoe::no_encryption_required
    >;

    using service_2 = bluetoe::service<
        bluetoe::requires_encryption,
        service_uuid_b,
        char_no_enc
    >;

    using server = bluetoe::server<
        service_1,
        service_2
    >;

    BOOST_CHECK( !bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service_1, server >::value ));
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_no_enc, service_2, server >::value ));
}

BOOST_AUTO_TEST_CASE( server_may_require_encryption )
{
    using service = bluetoe::service<
        service_uuid,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::may_require_encryption,
        service
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service, server >::value ));
}

BOOST_AUTO_TEST_CASE( service_may_require_encryption )
{
    using service = bluetoe::service<
        bluetoe::may_require_encryption,
        service_uuid,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        service
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service, server >::value ));
}

BOOST_AUTO_TEST_CASE( characteristic_may_require_encryption )
{
    using char_1 = bluetoe::characteristic<
        bluetoe::bind_characteristic_value< decltype( dummy_val ), &dummy_val >,
        char_uuid,
        bluetoe::may_require_encryption
    >;

    using service = bluetoe::service<
        bluetoe::no_encryption_required,
        service_uuid,
        char_1
    >;

    using server = bluetoe::server<
        bluetoe::no_encryption_required,
        service
    >;

    BOOST_CHECK( bluetoe::details::requires_encryption_support_t< server >::value );
    BOOST_CHECK(( !bluetoe::details::characteristic_requires_encryption< char_1, service, server >::value ));
}
