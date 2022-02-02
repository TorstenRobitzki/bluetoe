#ifndef BLUETOE_BINDINGS_NRF52_HPP
#define BLUETOE_BINDINGS_NRF52_HPP

#include <bluetoe/link_layer.hpp>
#include <bluetoe/nrf.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>

namespace bluetoe
{
    namespace nrf52_details
    {
        class radio_hardware
        {

        };

        template < typename ... Options >
        struct radio_options
        {
            using result = typename bluetoe::details::find_all_by_meta_type<
                bluetoe::nrf::details::nrf52_radio_option_meta_type,
                Options...
            >::type;
        };

        template < typename ... Options >
        struct link_layer_options
        {
            using result = typename bluetoe::details::find_all_by_not_meta_type<
                bluetoe::details::binding_option_meta_type,
                Options...
            >::type;
        };

        class nrf52_radio_base
        {
        public:
            void schedule_advertisment(
                unsigned                                    channel,
                const bluetoe::link_layer::write_buffer&    advertising_data,
                const bluetoe::link_layer::write_buffer&    response_data,
                bluetoe::link_layer::delta_time             when,
                const bluetoe::link_layer::read_buffer&     receive );

            void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );
            std::uint32_t static_random_address_seed() const;
            void run();
            void wake_up();

            class lock_guard
            {
            public:
                lock_guard();
                ~lock_guard();

                lock_guard( const lock_guard& ) = delete;
                lock_guard& operator=( const lock_guard& ) = delete;
            private:
                const std::uint32_t context_;
            };
        };

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            bool EnabledEncryption,
            class CallBack,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio;

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBack,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio< TransmitSize, ReceiveSize, false, CallBack, Hardware, RadioOptions... > :
            public nrf52_radio_base,
            public bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, nrf52_radio< TransmitSize, ReceiveSize, false, CallBack, Hardware, RadioOptions... > >
        {
        public:
            static constexpr bool hardware_supports_encryption = false;
        };

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBack,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio< TransmitSize, ReceiveSize, true, CallBack, Hardware, RadioOptions... > :
            public nrf52_radio_base,
            public bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, nrf52_radio< TransmitSize, ReceiveSize, true, CallBack, Hardware, RadioOptions... > >
        {
        public:
            static constexpr bool hardware_supports_lesc_pairing = true;
            static constexpr bool hardware_supports_legacy_pairing = true;
            static constexpr bool hardware_supports_encryption = hardware_supports_lesc_pairing || hardware_supports_legacy_pairing;

            /**
             * security tool box required by legacy pairing
             */
            bluetoe::details::uint128_t create_srand();

            bluetoe::details::longterm_key_t create_long_term_key();

            bluetoe::details::uint128_t c1(
                const bluetoe::details::uint128_t& temp_key,
                const bluetoe::details::uint128_t& rand,
                const bluetoe::details::uint128_t& p1,
                const bluetoe::details::uint128_t& p2 ) const;

            bluetoe::details::uint128_t s1(
                const bluetoe::details::uint128_t& temp_key,
                const bluetoe::details::uint128_t& srand,
                const bluetoe::details::uint128_t& mrand );

            /**
             * security tool box required by LESC pairing
             */
            bool is_valid_public_key( const std::uint8_t* public_key ) const;

            std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > generate_keys();

            bluetoe::details::uint128_t select_random_nonce();

            bluetoe::details::ecdh_shared_secret_t p256( const std::uint8_t* private_key, const std::uint8_t* public_key );

            bluetoe::details::uint128_t f4( const std::uint8_t* u, const std::uint8_t* v, const std::array< std::uint8_t, 16 >& k, std::uint8_t z );

            std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > f5(
                const bluetoe::details::ecdh_shared_secret_t dh_key,
                const bluetoe::details::uint128_t& nonce_central,
                const bluetoe::details::uint128_t& nonce_periperal,
                const bluetoe::link_layer::device_address& addr_controller,
                const bluetoe::link_layer::device_address& addr_peripheral );

            bluetoe::details::uint128_t f6(
                const bluetoe::details::uint128_t& key,
                const bluetoe::details::uint128_t& n1,
                const bluetoe::details::uint128_t& n2,
                const bluetoe::details::uint128_t& r,
                const bluetoe::details::io_capabilities_t& io_caps,
                const bluetoe::link_layer::device_address& addr_controller,
                const bluetoe::link_layer::device_address& addr_peripheral );

            std::uint32_t g2(
                const std::uint8_t*                 u,
                const std::uint8_t*                 v,
                const bluetoe::details::uint128_t&  x,
                const bluetoe::details::uint128_t&  y );

            /**
             * Functions required by IO capabilties
             */
            bluetoe::details::uint128_t create_passkey();
        };

        template < typename Server, bool EnabledEncryption, typename RadioOptions, typename LinkLayerOptions >
        struct link_layer_factory;

        template < typename Server, bool EnabledEncryption, typename ... RadioOptions, typename ... LinkLayerOptions >
        struct link_layer_factory< Server, EnabledEncryption, std::tuple< RadioOptions... >, std::tuple< LinkLayerOptions... > >
        {
            template <
                std::size_t TransmitSize,
                std::size_t ReceiveSize,
                class CallBack
            >
            using radio_t = nrf52_radio< TransmitSize, ReceiveSize, EnabledEncryption, CallBack, radio_hardware, RadioOptions... >;

            using link_layer = bluetoe::link_layer::link_layer< Server, radio_t, LinkLayerOptions... >;
        };
    }

    /**
     * @brief binding for nRF52 microcontrollers
     *
     * Options that are ment to configure the nRF52 bindings are:
     * bluetoe::nrf::sleep_clock_crystal_oscillator
     * bluetoe::nrf::calibrated_sleep_clock
     * bluetoe::nrf::synthesized_sleep_clock
     */
    template < class Server, typename ... Options >
    using nrf52 = typename nrf52_details::link_layer_factory<
        Server,
        /*
        typename details::requires_encryption_support_t< Server >::value*/ false ,
        int, int >::link_layer;
        // typename nrf52_details::radio_options< Options... >::result,
        // typename nrf52_details::link_layer_options< Options... >::result >::link_layer;
}

#endif
