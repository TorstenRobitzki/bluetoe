#ifndef BLUETOE_BINDINGS_NRF_HPP
#define BLUETOE_BINDINGS_NRF_HPP

#include <bluetoe/meta_types.hpp>
#include <bluetoe/default_pdu_layout.hpp>

#include <nrf.h>

namespace bluetoe
{
    /**
     * @brief namespace with nRF51/52 specific configuration options
     */
    namespace nrf
    {
        /*
         * Some aliases that can be used in the debugger
         */
        static NRF_RADIO_Type* const        nrf_radio            = NRF_RADIO;
        static NRF_TIMER_Type* const        nrf_timer            = NRF_TIMER0;
        static NRF_CLOCK_Type* const        nrf_clock            = NRF_CLOCK;
        static NRF_RTC_Type* const          nrf_rtc              = NRF_RTC0;
        static NRF_CCM_Type* const          nrf_ccm              = NRF_CCM;
        static NRF_AAR_Type* const          nrf_aar              = NRF_AAR;
        static NRF_PPI_Type* const          nrf_ppi              = NRF_PPI;
        static NRF_RNG_Type* const          nrf_random           = NRF_RNG;
        static NRF_ECB_Type* const          nrf_aes              = NRF_ECB;
        static NRF_GPIOTE_Type* const       nrf_gpiote           = NRF_GPIOTE;
        static NVIC_Type* const             nvic                 = NVIC;

        namespace nrf_details {
            struct radio_option_meta_type : ::bluetoe::details::binding_option_meta_type {};
            struct sleep_clock_source_meta_type : radio_option_meta_type {};

            inline void start_lfclock_and_rtc()
            {
                nrf_clock->EVENTS_LFCLKSTARTED = 0;
                nrf_clock->TASKS_LFCLKSTART = 1;

                while ( nrf_clock->EVENTS_LFCLKSTARTED == 0 )
                    ;

                // https://infocenter.nordicsemi.com/topic/errata_nRF52840_Rev3/ERR/nRF52840/Rev3/latest/anomaly_840_20.html#anomaly_840_20
                nrf_rtc->TASKS_STOP = 0;
                nrf_rtc->TASKS_START = 1;

                // Configure the RTC to generate these two events
                nrf_rtc->EVTEN =
                    ( RTC_EVTEN_COMPARE0_Enabled << RTC_EVTEN_COMPARE0_Pos )
                  | ( RTC_EVTEN_COMPARE1_Enabled << RTC_EVTEN_COMPARE1_Pos );
            }
        }

        /**
         * @brief configure the low frequency clock to be sourced out of the high frequency clock
         *
         * The resulting sleep clock accurary is then the accuarcy of your high frequency clock source.
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::sleep_clock_crystal_oscillator
         * @sa bluetoe::nrf::calibrated_sleep_clock
         *
         * TODO: With this configuration, the HFXO must not be stopped.
         */
        struct synthesized_sleep_clock
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;

            static void start_clock()
            {
                nrf_clock->LFCLKSRC = CLOCK_LFCLKSRCCOPY_SRC_Synth << CLOCK_LFCLKSRCCOPY_SRC_Pos;
                nrf_details::start_lfclock_and_rtc();
            }
            /** @endcond */
        };

        /**
         * @brief configure the low frequency clock to be sourced from a crystal oscilator
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::synthesized_sleep_clock
         * @sa bluetoe::nrf::calibrated_sleep_clock
         */
        struct sleep_clock_crystal_oscillator
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;

            static void start_clock()
            {
                nrf_clock->LFCLKSRC = CLOCK_LFCLKSRCCOPY_SRC_Xtal << CLOCK_LFCLKSRCCOPY_SRC_Pos;
                nrf_details::start_lfclock_and_rtc();
            }
            /** @endcond */
        };

        /**
         * @brief configure the low frequency clock to run from the RC oscilator and to be
         *        calibrated, using the high frequency clock.
         *
         * According to the datasheet, the resulting sleep clock accuarcy is then 500ppm.
         * If no sleep clock configuration is given, this is the default.
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::synthesized_sleep_clock
         * @sa bluetoe::nrf::sleep_clock_crystal_oscillator
         */
        struct calibrated_sleep_clock
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;

            static void start_clock()
            {
            }
            /** @endcond */
        };
    }

    namespace nrf_details
    {
        struct encrypted_pdu_layout : bluetoe::link_layer::details::layout_base< encrypted_pdu_layout >
        {
            /** @cond HIDDEN_SYMBOLS */
            static constexpr std::size_t header_size = sizeof( std::uint16_t );

            using bluetoe::link_layer::details::layout_base< encrypted_pdu_layout >::header;

            static std::uint16_t header( const std::uint8_t* pdu )
            {
                return ::bluetoe::details::read_16bit( pdu );
            }

            static void header( std::uint8_t* pdu, std::uint16_t header_value )
            {
                ::bluetoe::details::write_16bit( pdu, header_value );
            }

            static std::pair< std::uint8_t*, std::uint8_t* > body( const link_layer::read_buffer& pdu )
            {
                assert( pdu.size >= header_size );

                return { &pdu.buffer[ header_size + 1 ], &pdu.buffer[ pdu.size ] };
            }

            static std::pair< const std::uint8_t*, const std::uint8_t* > body( const link_layer::write_buffer& pdu )
            {
                assert( pdu.size >= header_size );

                return { &pdu.buffer[ header_size + 1 ], &pdu.buffer[ pdu.size ] };
            }

            static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size )
            {
                return header_size + payload_size + 1;
            }
            /** @endcond */
        };
    }
}

#endif

