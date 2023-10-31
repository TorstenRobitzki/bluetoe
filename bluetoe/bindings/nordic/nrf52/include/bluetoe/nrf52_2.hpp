#ifndef BLUETOE_BINDINGS_NRF52_2_HPP
#define BLUETOE_BINDINGS_NRF52_2_HPP

#include <bluetoe/nrf.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>
#include <bluetoe/security_tool_box.hpp>
#include <bluetoe/connection_events.hpp>
#include <bluetoe/encryption.hpp>
#include <bluetoe/ll_options.hpp>
#include <bluetoe/abs_time.hpp>

#include <tuple>

namespace bluetoe
{
    namespace nrf52_details
    {
        struct radio_hardware_with_crypto_support
        {

        };

        struct radio_hardware_without_crypto_support
        {

        };

        template < typename ... Options >
        struct radio_options
        {
            using result = typename bluetoe::details::find_all_by_meta_type<
                bluetoe::nrf::nrf_details::radio_option_meta_type,
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

        template < class CallBacks, class Hardware, class Buffer, typename ... RadioOptions >
        class nrf52_radio_base : public Buffer
        {
        public:
            static link_layer::abs_time time_now()
            {
                return link_layer::abs_time();
            }

            void set_local_address( const bluetoe::link_layer::device_address& address )
            {
                (void)address;
            }

            bool schedule_advertising_event(
                std::uint32_t                               channel,
                bluetoe::link_layer::abs_time               when,
                const bluetoe::link_layer::write_buffer&    advertising_data,
                const bluetoe::link_layer::write_buffer&    response_data,
                const bluetoe::link_layer::read_buffer&     receive )
            {
                (void)channel;
                (void)when;
                (void)advertising_data;
                (void)response_data;
                (void)receive;

                return false;
            }

            void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
            {
                (void)access_address;
                (void)crc_init;
            }

            void set_phy(
                bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t receiving_encoding,
                bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t transmiting_c_encoding )
            {
                (void)receiving_encoding;
                (void)transmiting_c_encoding;
            }

            bool schedule_connection_event(
                std::uint32_t                   channel,
                bluetoe::link_layer::abs_time   start,
                bluetoe::link_layer::abs_time   end )
            {
                (void)channel;
                (void)start;
                (void)end;
                return false;
            }

            void cancel_radio_event()
            {}

            static constexpr bool hardware_supports_2mbit = true;
            static constexpr bool hardware_supports_synchronized_user_timer = true;

            // TODO
            static constexpr std::size_t radio_max_supported_payload_length = 255u;
            // TODO
            static constexpr std::uint32_t sleep_time_accuracy_ppm = 20u;
        };

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            bool EnabledEncryption,
            class CallBacks,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio;

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBacks,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio< TransmitSize, ReceiveSize, false, CallBacks, Hardware, RadioOptions... > :
            public nrf52_radio_base< CallBacks, Hardware, bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, nrf52_radio< TransmitSize, ReceiveSize, false, CallBacks, Hardware, RadioOptions... > >,
                                    RadioOptions... >
        {
        public:
            static constexpr bool hardware_supports_encryption = false;

            bluetoe::link_layer::radio_properties properties() const
            {
                return bluetoe::link_layer::radio_properties( *this );
            }
        };

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBacks,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > :
            public nrf52_radio_base<
                CallBacks,
                Hardware,
                bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize,
                    nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > >,
                    RadioOptions... >,
            public security_tool_box
        {
        public:
            static constexpr bool hardware_supports_lesc_pairing    = true;
            static constexpr bool hardware_supports_legacy_pairing  = true;
            static constexpr bool hardware_supports_encryption      = hardware_supports_lesc_pairing || hardware_supports_legacy_pairing;

            using radio_base_t = nrf52_radio_base<
                CallBacks,
                Hardware,
                bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize,
                    nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > >,
                    RadioOptions...  >;

            bluetoe::link_layer::radio_properties properties() const
            {
                return bluetoe::link_layer::radio_properties( *this );
            }
        };

        template < typename ... Options >
        struct buffer_sizes
        {
            typedef typename ::bluetoe::details::find_by_meta_type<
                bluetoe::link_layer::details::buffer_sizes_meta_type,
                Options...,
                ::bluetoe::link_layer::buffer_sizes<>  // default
            >::type s_type;

            static constexpr std::size_t tx_size = s_type::transmit_buffer_size;
            static constexpr std::size_t rx_size = s_type::receive_buffer_size;
        };

        template < bool EnabledEncryption, typename LinkLayerOptions >
        struct radio_factory;

        template < bool EnabledEncryption, typename ... LinkLayerOptions >
        struct radio_factory< EnabledEncryption, std::tuple< LinkLayerOptions... > >
        {
            using radio_hardware_t = typename details::select_type<
                EnabledEncryption,
                radio_hardware_with_crypto_support,
                radio_hardware_without_crypto_support >::type;

            using buffer_sizes_t = buffer_sizes< LinkLayerOptions... >;

            template < typename CallBacks, typename... Options >
            using radio_t = nrf52_radio< buffer_sizes_t::tx_size, buffer_sizes_t::rx_size, EnabledEncryption, CallBacks, radio_hardware_t, Options... >;
        };

    }

    /**
     * @brief radio binding for nRF52 microcontrollers
     *
     * Options that are ment to configure the nRF52 bindings are:
     * bluetoe::nrf::sleep_clock_crystal_oscillator
     * bluetoe::nrf::calibrated_sleep_clock
     * bluetoe::nrf::synthesized_sleep_clock
     */
    template < bool EnabledEncryption, typename ... Options >
    using nrf52_radio = typename nrf52_details::radio_factory<
        EnabledEncryption,
        typename nrf52_details::radio_options< Options... >::result
    >;
}

#endif
