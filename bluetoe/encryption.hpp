#ifndef BLUETOE_ENCRYPTION_HPP
#define BLUETOE_ENCRYPTION_HPP

#include <bluetoe/server.hpp>
#include <bluetoe/ll_options.hpp>

namespace bluetoe {

    namespace details {
        struct requires_encryption_meta_type {};
        struct no_encryption_required_meta_type {};
        struct may_require_encryption_meta_type {};
    }

    /**
     * @brief defines that access to characteristic(s) require an encrypted link without
     *        MITM Protection.
     *
     * Can be used on server, service or characteristic level to define that the access
     * to a characteristic requires an encrypted link. If the link is not encrypted,
     * when accessing the characteristic, Bluetoe replies with an ATT Insufficient Authorization
     * error, if the requesting device is not paired and ATT Insufficient Encryption, if the
     * device is paired, but the link is not encrypted.
     * (see table 10.2 Vol 3, Part C, 10.3.1)
     *
     * If applied to a server or service definition, this definition applies to all containing
     * characteristics, where it can be overridden.
     *
     * Example
     * @code
    char simple_value = 0;
    constexpr char name[] = "This is the name of the characteristic";

    using server = bluetoe::server<
        // By default, require encryption for all characteristics of
        // all services.
        bluetoe::requires_encryption,
        // Service A
        bluetoe::service<
            // But for this service, no encryption is required
            bluetoe::no_encryption_required,
            service_uuid_a,
            ...
        >,
        // Service B
        bluetoe::service<
            service_uuid_b,
            bluetoe::characteristic<
                char_uuid,
                // For all characteristics of this service, but this characteristic,
                // encryption is required
                bluetoe::no_encryption_required
            >,
            ...
        >,
        // Service C
    >;
     * @endcode
     *
     * @sa characteristic
     * @sa service
     * @sa server
     * @sa no_encryption_required
     * @sa may_require_encryption
     */
    struct requires_encryption {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::requires_encryption_meta_type,
            details::valid_server_option_meta_type,
            details::valid_service_option_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief defines that access to characteristic(s) does not require an encrypted link.
     *
     * If applied to a server or service definition, this definition applies to all containing
     * characteristics, where it can be overridden.
     *
     * If no characteristic in the entire server requires encryption, Bluetoe will leave out
     * the support code for encryption.
     *
     * @sa requires_encryption
     * @sa may_require_encryption
     */
    struct no_encryption_required {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::no_encryption_required_meta_type,
            details::valid_server_option_meta_type,
            details::valid_service_option_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief defines that a characteristic may require encyption
     *
     * The declaration forces bluetoe add the required code for encrypting
     * links. Basecally this means that a custom characterstic read or write handler
     * may require encryption under specific circumstances.
     *
     * This option can be passed to a server, service or a characteristic declaration.
     *
     * @sa requires_encryption
     * @sa no_encryption_required
     */
    struct may_require_encryption {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::may_require_encryption_meta_type,
            details::valid_server_option_meta_type,
            details::valid_service_option_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    namespace details {

        template < typename Server, bool Default = false >
        struct requires_encryption_support_t
        {
            static bool constexpr value = Default;
        };

        template < typename ... Options, typename T >
        struct requires_encryption_support_t< std::tuple< T, Options... >, false >
        {
            static bool constexpr value =
                requires_encryption_support_t< T, false >::value
             || requires_encryption_support_t< std::tuple< Options... >, false >::value;
        };

        template < typename ... Options, typename T >
        struct requires_encryption_support_t< std::tuple< T, Options... >, true >
        {
            static bool constexpr value =
                requires_encryption_support_t< T, true >::value
             && requires_encryption_support_t< std::tuple< Options... >, true >::value;
        };

        template < bool Default, typename ... Options >
        struct encryption_default
        {
            static bool constexpr require_encryption =
                details::has_option< requires_encryption, Options... >::value
             || details::has_option< may_require_encryption, Options... >::value;

            static bool constexpr require_not_encryption =
                 details::has_option< no_encryption_required, Options... >::value;

            // require !require default | value
            //    true    false   false |  true
            //   false     true   false | false
            //   false    false   false | false
            //    true    false    true |  true
            //   false     true    true | false
            //   false    false    true |  true
            static bool constexpr value =
                ( require_encryption && !require_not_encryption )
             || ( !require_encryption && !require_not_encryption && Default );

        };

        template < typename ... Options, bool Default >
        struct requires_encryption_support_t< bluetoe::server< Options... >, Default > {
            static bool constexpr default_val = encryption_default< Default, Options... >::value;

            static bool constexpr value =
                requires_encryption_support_t<
                    typename bluetoe::server< Options... >::services,
                    default_val >::value;
        };

        template < typename ... Options, bool Default >
        struct requires_encryption_support_t< bluetoe::service< Options... >, Default > {
            static bool constexpr default_val = encryption_default< Default, Options... >::value;

            static bool constexpr value =
                requires_encryption_support_t<
                    typename bluetoe::service< Options... >::characteristics,
                    default_val >::value;
        };

        template < typename ... Options, bool Default >
        struct requires_encryption_support_t< bluetoe::characteristic< Options... >, Default > {
            // @TODO the default for a characteristic should always be true
            static bool constexpr value = encryption_default< Default, Options... >::value;
        };
    }

} // namespace bluetoe

#endif
