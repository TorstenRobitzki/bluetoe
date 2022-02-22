#include <bluetoe/nrf52.hpp>

#include <cstring>
#include <algorithm>

#include "uECC.h"

namespace bluetoe
{
using namespace bluetoe::nrf;

namespace nrf52_details
{
    static constexpr std::size_t        radio_address_ccm_crypt         = 25;
    static constexpr std::size_t        radio_end_capture2_ppi_channel  = 27;
    static constexpr std::size_t        compare0_txen_ppi_channel       = 20;
    static constexpr std::size_t        compare0_rxen_ppi_channel       = 21;
    static constexpr std::size_t        compare1_disable_ppi_channel    = 22;
    static constexpr std::size_t        radio_bcmatch_aar_start_channel = 23;

    static constexpr std::uint32_t      all_preprogramed_ppi_channels_mask =
        ( 1 << radio_address_ccm_crypt )
      | ( 1 << radio_end_capture2_ppi_channel )
      | ( 1 << compare0_txen_ppi_channel )
      | ( 1 << compare0_rxen_ppi_channel )
      | ( 1 << compare1_disable_ppi_channel )
      | ( 1 << radio_bcmatch_aar_start_channel );

#   if defined BLUETOE_NRF52_RADIO_DEBUG
        static constexpr int debug_pin_end_crypt     = 11;
        static constexpr int debug_pin_ready_disable = 12;
        static constexpr int debug_pin_address_end   = 13;
        static constexpr int debug_pin_keystream     = 14;
        static constexpr int debug_pin_debug         = 15;
        static constexpr int debug_pin_isr           = 16;

        void init_debug()
        {
            for ( auto pin : { debug_pin_end_crypt, debug_pin_ready_disable,
                                debug_pin_address_end, debug_pin_keystream, debug_pin_debug,
                                debug_pin_isr } )
            {
                NRF_GPIO->PIN_CNF[ pin ] =
                    ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
                    ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );
            }

            NRF_GPIOTE->CONFIG[ 0 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_address_end << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_GPIOTE->CONFIG[ 1 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_keystream << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_GPIOTE->CONFIG[ 2 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_ready_disable << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );

            NRF_GPIOTE->CONFIG[ 3 ] =
                ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos ) |
                ( debug_pin_end_crypt << GPIOTE_CONFIG_PSEL_Pos ) |
                ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos ) |
                ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );


            NRF_PPI->CH[ 0 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_ADDRESS );
            NRF_PPI->CH[ 0 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 0 ] );

            NRF_PPI->CH[ 1 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_END );
            NRF_PPI->CH[ 1 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 0 ] );

            NRF_PPI->CH[ 2 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_CCM->EVENTS_ENDCRYPT );
            NRF_PPI->CH[ 2 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 3 ] );

            NRF_PPI->CH[ 3 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_CCM->EVENTS_ENDKSGEN );
            NRF_PPI->CH[ 3 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_CLR[ 1 ] );

            NRF_PPI->CH[ 4 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_READY );
            NRF_PPI->CH[ 4 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 2 ] );

            NRF_PPI->CH[ 5 ].EEP = reinterpret_cast< std::uint32_t >( &NRF_RADIO->EVENTS_DISABLED );
            NRF_PPI->CH[ 5 ].TEP = reinterpret_cast< std::uint32_t >( &NRF_GPIOTE->TASKS_OUT[ 2 ] );

            NRF_PPI->CHENSET = 0x3f;
        }

        void toggle_debug_pin()
        {
            NRF_GPIO->OUTSET = ( 1 << debug_pin_debug );
            NRF_GPIO->OUTCLR = ( 1 << debug_pin_debug );
        }

        void set_isr_pin()
        {
            NRF_GPIO->OUTSET = ( 1 << debug_pin_isr );
        }

        void reset_isr_pin()
        {
            NRF_GPIO->OUTCLR = ( 1 << debug_pin_isr );
        }
#   else
        void init_debug() {}

        void toggle_debug_pin() {}
        void set_isr_pin() {}
        void reset_isr_pin() {}
#   endif

    /*
     * Frequency correction if NRF_FICR_Type has an OVERRIDEEN field
     */
    template < typename F, typename R >
    static auto override_correction(F* ficr, R* radio) -> decltype(F::OVERRIDEEN)
    {
#       ifndef FICR_OVERRIDEEN_BLE_1MBIT_Msk
#           define FICR_OVERRIDEEN_BLE_1MBIT_Msk 1
#       endif
#       ifndef FICR_OVERRIDEEN_BLE_1MBIT_Override
#           define FICR_OVERRIDEEN_BLE_1MBIT_Override 1
#       endif
#       ifndef FICR_OVERRIDEEN_BLE_1MBIT_Pos
#           define FICR_OVERRIDEEN_BLE_1MBIT_Pos 1
#       endif

        if ( ( ficr->OVERRIDEEN & FICR_OVERRIDEEN_BLE_1MBIT_Msk ) == (FICR_OVERRIDEEN_BLE_1MBIT_Override << FICR_OVERRIDEEN_BLE_1MBIT_Pos) )
        {
            radio->OVERRIDE0 = ficr->BLE_1MBIT[0];
            radio->OVERRIDE1 = ficr->BLE_1MBIT[1];
            radio->OVERRIDE2 = ficr->BLE_1MBIT[2];
            radio->OVERRIDE3 = ficr->BLE_1MBIT[3];
            radio->OVERRIDE4 = ficr->BLE_1MBIT[4] | 0x80000000;
        }

        return ficr->OVERRIDEEN;
    }

    template < typename F >
    static void override_correction(...)
    {
    }

    static void init_radio( bool encryption_possible )
    {
// TODO Use MODECNF0.RU to speedup rampup?
        override_correction< NRF_FICR_Type >( NRF_FICR, nrf_radio );

        nrf_radio->MODE  = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;

        nrf_radio->PCNF0 =
            ( 1 << RADIO_PCNF0_S0LEN_Pos ) |
            ( 8 << RADIO_PCNF0_LFLEN_Pos ) |
            ( 0 << RADIO_PCNF0_S1LEN_Pos ) |
            ( encryption_possible
                ? ( RADIO_PCNF0_S1INCL_Include << RADIO_PCNF0_S1INCL_Pos )
                : ( RADIO_PCNF0_S1INCL_Automatic << RADIO_PCNF0_S1INCL_Pos ) );

        nrf_radio->PCNF1 =
            ( RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos ) |
            ( RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos ) |
            ( 3 << RADIO_PCNF1_BALEN_Pos ) |
            ( 0 << RADIO_PCNF1_STATLEN_Pos );

        nrf_radio->TXADDRESS = 0;
        nrf_radio->RXADDRESSES = 1 << 0;

        nrf_radio->CRCCNF    =
            ( RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos ) |
            ( RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos );

        // clear all used PPI pre-programmed channels (16.1.1)
        nrf_ppi->CHENCLR = all_preprogramed_ppi_channels_mask;

        // The polynomial has the form of x^24 +x^10 +x^9 +x^6 +x^4 +x^3 +x+1
        nrf_radio->CRCPOLY   = 0x100065B;

        // TIFS is only enforced if END_DISABLE and DISABLED_TXEN shortcuts are enabled.
        nrf_radio->TIFS      = 150;
    }

    static void init_timer()
    {
        nrf_timer->MODE        = TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos;
        nrf_timer->BITMODE     = TIMER_BITMODE_BITMODE_32Bit;
        nrf_timer->PRESCALER   = 4; // resulting in a timer resolution of 1µs

        nrf_timer->TASKS_STOP  = 1;
        nrf_timer->TASKS_CLEAR = 1;
        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 1 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 2 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 3 ] = 0;
        nrf_timer->INTENCLR    = 0xffffffff;

        nrf_timer->TASKS_START = 1;
    }

    static void start_high_frequency_clock()
    {
        // start high freuquence clock source if not done yet
        if ( !nrf_clock->EVENTS_HFCLKSTARTED )
        {
            nrf_clock->TASKS_HFCLKSTART = 1;

            // TODO: do not wait busy
            // Issue: do not poll for readiness of the high frequency clock #63
            while ( !nrf_clock->EVENTS_HFCLKSTARTED )
                ;
        }
    }

    static void* instance = nullptr;
    static void (*isr_handler)( void* );

    static void enable_interrupts( void (*isr)( void* ), void* that )
    {
        instance    = that;
        isr_handler = isr;

        NVIC_SetPriority( RADIO_IRQn, 0 );
        NVIC_ClearPendingIRQ( RADIO_IRQn );
        NVIC_EnableIRQ( RADIO_IRQn );
    }

    static unsigned frequency_from_channel( unsigned channel )
    {
        assert( channel < 40 );

        if ( channel <= 10 )
            return 4 + 2 * channel;

        if ( channel <= 36 )
            return 6 + 2 * channel;

        if ( channel == 37 )
            return 2;

        if ( channel == 38 )
            return 26;

        return 80;
    }

    //////////////////////////////////////////
    // class counter
    counter::counter()
        : low( 0 )
        , high( 0 )
    {
    }

    void counter::increment()
    {
        ++low;

        if ( low == 0 )
            ++high;
    }

    void counter::copy_to( std::uint8_t* target ) const
    {
        target  = details::write_32bit( target, low );
        *target = high;
    }

    /////////////////////////////////
    // class nrf52_security_tool_box

    // TODO: Entropie-Pool to not block here too much
    // How much random numbers do we need:
    //  - without encryption?
    //  - encrytped but already paired? <-- this might be the sweetspot
    //  - leagacy pairing?
    //  - LESC pairing?
    static std::uint8_t random_number8()
    {
        nrf_random->TASKS_START = 1;

        while ( !nrf_random->EVENTS_VALRDY )
            ;

        nrf_random->EVENTS_VALRDY = 0;

        return nrf_random->VALUE;
    }

    static std::uint16_t random_number16()
    {
        return static_cast< std::uint16_t >( random_number8() )
            | ( static_cast< std::uint16_t >( random_number8() ) << 8 );
    }

    static std::uint32_t random_number32()
    {
        return static_cast< std::uint32_t >( random_number16() )
            | ( static_cast< std::uint32_t >( random_number16() ) << 16 );
    }

    static std::uint64_t random_number64()
    {
        return static_cast< std::uint64_t >( random_number32() )
            | ( static_cast< std::uint64_t >( random_number32() ) << 32 );
    }

    // Note: While every function above the link layer uses low to high byte order inputs to the
    // EAS function, the CCM, that encrypts the link layer trafic, uses high to low byte order.
    // That's why the intput and output in aes_le() changeing the byte order. The result of the
    // key deversification is the key for the CCM algorithm and thus have to be stored in high to
    // low byte order.

    static bluetoe::details::uint128_t aes_le( const bluetoe::details::uint128_t& key, const std::uint8_t* data )
    {
        static struct alignas( 4 ) ecb_data_t {
            std::uint8_t data[ 3 * 16 ];
        } ecb_scratch_data;

        nrf_aes->ECBDATAPTR = reinterpret_cast< std::uint32_t >( &ecb_scratch_data.data[ 0 ] );

        std::copy( key.rbegin(), key.rend(), &ecb_scratch_data.data[ 0 ] );
        std::reverse_copy( data, data + 16, &ecb_scratch_data.data[ 16 ] );

        nrf_aes->TASKS_STARTECB = 1;

        while ( !nrf_aes->EVENTS_ENDECB && !nrf_aes->EVENTS_ERRORECB )
            ;

        assert( !nrf_aes->EVENTS_ERRORECB );
        nrf_aes->EVENTS_ENDECB = 0;

        bluetoe::details::uint128_t result;
        std::copy( &ecb_scratch_data.data[ 32 ], &ecb_scratch_data.data[ 48 ], result.rbegin() );

        // erase key out of memory
        std::fill( &ecb_scratch_data.data[ 0 ], &ecb_scratch_data.data[ 16 ], 0 );

        return result;
    }

    static bluetoe::details::uint128_t aes_le( const bluetoe::details::uint128_t& key, const bluetoe::details::uint128_t& data )
    {
        return aes_le( key, data.data() );
    }

    static bluetoe::details::uint128_t xor_( bluetoe::details::uint128_t a, const std::uint8_t* b )
    {
        std::transform(
            a.begin(), a.end(),
            b,
            a.begin(),
            []( std::uint8_t a, std::uint8_t b ) -> std::uint8_t
            {
                return a xor b;
            }
        );

        return a;
    }

    static bluetoe::details::uint128_t xor_( bluetoe::details::uint128_t a, const bluetoe::details::uint128_t& b )
    {
        return xor_( a, b.data() );
    }

    bluetoe::details::uint128_t nrf52_security_tool_box::create_srand()
    {
        details::uint128_t result;
        std::generate( result.begin(), result.end(), random_number8 );

        return result;
    }

    bluetoe::details::longterm_key_t nrf52_security_tool_box::create_long_term_key()
    {
        const details::longterm_key_t result = {
            create_srand(),
            random_number64(),
            random_number16()
        };

        return result;
    }

    bluetoe::details::uint128_t nrf52_security_tool_box::c1(
        const bluetoe::details::uint128_t& temp_key,
        const bluetoe::details::uint128_t& rand,
        const bluetoe::details::uint128_t& p1,
        const bluetoe::details::uint128_t& p2 ) const
    {
        // c1 (k, r, preq, pres, iat, rat, ia, ra) = e(k, e(k, r XOR p1) XOR p2)
        const auto p1_ = aes_le( temp_key, xor_( rand, p1 ) );

        return aes_le( temp_key, xor_( p1_, p2 ) );
    }

    bluetoe::details::uint128_t nrf52_security_tool_box::s1(
        const bluetoe::details::uint128_t& temp_key,
        const bluetoe::details::uint128_t& srand,
        const bluetoe::details::uint128_t& mrand )
    {
        bluetoe::details::uint128_t r;
        std::copy( &srand[ 0 ], &srand[ 8 ], &r[ 8 ] );
        std::copy( &mrand[ 0 ], &mrand[ 8 ], &r[ 0 ] );

        return aes_le( temp_key, r );
    }

    bool nrf52_security_tool_box::is_valid_public_key( const std::uint8_t* public_key ) const
    {
        bluetoe::details::ecdh_public_key_t key;
        std::reverse_copy(public_key, public_key + 32, key.begin());
        std::reverse_copy(public_key + 32, public_key + 64, key.begin() + 32);

        return uECC_valid_public_key( key.data() );
    }

    std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > nrf52_security_tool_box::generate_keys()
    {
        uECC_set_rng( []( uint8_t *dest, unsigned size )->int {
            std::generate( dest, dest + size, random_number8 );

            return 1;
        } );

        bluetoe::details::ecdh_public_key_t  public_key;
        bluetoe::details::ecdh_private_key_t private_key;

        const auto rc = uECC_make_key( public_key.data(), private_key.data() );
        static_cast< void >( rc );
        assert( rc == 1 );

        std::reverse( public_key.begin(), public_key.begin() + 32 );
        std::reverse( public_key.begin() + 32, public_key.end() );
        std::reverse( private_key.begin(), private_key.end() );

        return { public_key, private_key };
    }

    bluetoe::details::uint128_t nrf52_security_tool_box::select_random_nonce()
    {
        bluetoe::details::uint128_t result;
        std::generate( result.begin(), result.end(), random_number8 );

        return result;
    }

    bluetoe::details::ecdh_shared_secret_t nrf52_security_tool_box::p256( const std::uint8_t* private_key, const std::uint8_t* public_key )
    {
        bluetoe::details::ecdh_private_key_t shared_secret;
        bluetoe::details::ecdh_private_key_t priv_key;
        bluetoe::details::ecdh_public_key_t  pub_key;
        std::reverse_copy( public_key, public_key + 32, pub_key.begin() );
        std::reverse_copy( public_key + 32, public_key + 64, pub_key.begin() + 32 );
        std::reverse_copy( private_key, private_key + 32, priv_key.begin() );

        const int rc = uECC_shared_secret( pub_key.data(), priv_key.data(), shared_secret.data() );
        static_cast< void >( rc );
        assert( rc == 1 );

        bluetoe::details::ecdh_shared_secret_t result;
        std::reverse_copy( shared_secret.begin(), shared_secret.end(), result.begin() );

        return result;
    }

    static bluetoe::details::uint128_t left_shift(const bluetoe::details::uint128_t& input)
    {
        bluetoe::details::uint128_t output;

        std::uint8_t overflow = 0;
        for ( std::size_t i = 0; i != input.size(); ++i )
        {
            output[ i ] = ( input[i] << 1 ) | overflow;
            overflow = ( input[ i ] & 0x80 ) ? 1 : 0;
        }

        return output;
    }

    static bluetoe::details::uint128_t aes_cmac_k1_subkey_generation( const bluetoe::details::uint128_t& key )
    {
        const bluetoe::details::uint128_t zero = {{ 0x00 }};
        const bluetoe::details::uint128_t C    = {{ 0x87 }};

        const bluetoe::details::uint128_t k0 = aes_le( key, zero );

        const bluetoe::details::uint128_t k1 = ( k0.back() & 0x80 ) == 0
            ? left_shift(k0)
            : xor_( left_shift(k0), C );

        return k1;
    }

    static bluetoe::details::uint128_t aes_cmac_k2_subkey_generation( const bluetoe::details::uint128_t& key )
    {
        const bluetoe::details::uint128_t C    = {{ 0x87 }};

        const bluetoe::details::uint128_t k1 = aes_cmac_k1_subkey_generation( key );
        const bluetoe::details::uint128_t k2 = ( k1.back() & 0x80 ) == 0
            ? left_shift(k1)
            : xor_( left_shift(k1), C );

        return k2;
    }

    bluetoe::details::uint128_t nrf52_security_tool_box::f4( const std::uint8_t* u, const std::uint8_t* v, const std::array< std::uint8_t, 16 >& k, std::uint8_t z )
    {
        const bluetoe::details::uint128_t m4 = {{
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, z
        }};

        auto t0 = aes_le( k, &u[16] );
        auto t1 = aes_le( k, xor_( t0, &u[0] ) );
        auto t2 = aes_le( k, xor_( t1, &v[16] ) );
        auto t3 = aes_le( k, xor_( t2, &v[0] ) );

        return aes_le( k, xor_( t3, xor_( aes_cmac_k2_subkey_generation( k ), m4 ) ) );
    }

    static bluetoe::details::uint128_t f5_cmac(
        const bluetoe::details::uint128_t& key,
        const std::uint8_t* buffer )
    {
        const std::uint8_t* m0 = &buffer[ 48 ];
        const std::uint8_t* m1 = &buffer[ 32 ];
        const std::uint8_t* m2 = &buffer[ 16 ];
        const std::uint8_t* m3 = &buffer[ 0 ];

        auto t0 = aes_le( key, m0 );
        auto t1 = aes_le( key, xor_( t0, m1 ) );
        auto t2 = aes_le( key, xor_( t1, m2 ) );

        return aes_le( key, xor_( t2, xor_( aes_cmac_k2_subkey_generation( key ), m3 ) ) );
    }

    static bluetoe::details::uint128_t f5_key( const bluetoe::details::ecdh_shared_secret_t dh_key )
    {
        static const bluetoe::details::uint128_t salt = {{
            0xBE, 0x83, 0x60, 0x5A, 0xDB, 0x0B, 0x37, 0x60,
            0x38, 0xA5, 0xF5, 0xAA, 0x91, 0x83, 0x88, 0x6C
        }};

        auto t0 = aes_le( salt, &dh_key[ 16 ] );

        return aes_le( salt, xor_( t0, xor_( aes_cmac_k1_subkey_generation( salt ), &dh_key[ 0 ] ) ) );
    }

    std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > nrf52_security_tool_box::f5(
        const bluetoe::details::ecdh_shared_secret_t dh_key,
        const bluetoe::details::uint128_t& nonce_central,
        const bluetoe::details::uint128_t& nonce_periperal,
        const bluetoe::link_layer::device_address& addr_controller,
        const bluetoe::link_layer::device_address& addr_peripheral )
    {
        // all 4 blocks are allocated in revers order to make it easier to copy data that overlaps
        // two blocks
        std::uint8_t buffer[ 64 ] = { 0 };

        static const std::uint8_t m0_fill[] = {
            0x65, 0x6c, 0x74, 0x62
        };

        static const std::uint8_t m3_fill[] = {
            0x80, 0x00, 0x01
        };

        std::copy( std::begin( m0_fill ), std::end( m0_fill ), &buffer[ 11 + 48 ] );
        std::copy( std::begin( m3_fill ), std::end( m3_fill ), &buffer[ 10 ] );

        std::copy( nonce_central.begin(), nonce_central.end(), &buffer[ 32 + 11 ] );
        std::copy( nonce_periperal.begin(), nonce_periperal.end(), &buffer[ 16 + 11 ] );

        buffer[ 16 + 10 ] = addr_controller.is_random() ? 1 : 0;
        std::copy( addr_controller.begin(), addr_controller.end(), &buffer[ 16 + 4 ] );
        buffer[ 16 + 3 ] = addr_peripheral.is_random() ? 1 : 0;
        std::copy( addr_peripheral.begin(), addr_peripheral.end(), &buffer[ 13 ] );

        const bluetoe::details::uint128_t key     = f5_key( dh_key );
        const bluetoe::details::uint128_t mac_key = f5_cmac( key, buffer );
        // increment counter
        buffer[ 15 + 48 ] = 1;
        const bluetoe::details::uint128_t ltk     = f5_cmac( key, buffer );

        return { mac_key, ltk };
    }

    bluetoe::details::uint128_t nrf52_security_tool_box::f6(
        const bluetoe::details::uint128_t& key,
        const bluetoe::details::uint128_t& n1,
        const bluetoe::details::uint128_t& n2,
        const bluetoe::details::uint128_t& r,
        const bluetoe::details::io_capabilities_t& io_caps,
        const bluetoe::link_layer::device_address& addr_controller,
        const bluetoe::link_layer::device_address& addr_peripheral )
    {
        std::uint8_t m4_m3[ 32 ] = { 0 };

        std::copy( io_caps.begin(), io_caps.end(), &m4_m3[ 16 + 13 ] );
        m4_m3[ 16 + 12 ] = addr_controller.is_random() ? 1 : 0;
        std::copy( addr_controller.begin(), addr_controller.end(), &m4_m3[ 22 ] );
        m4_m3[ 16 + 5 ] = addr_peripheral.is_random() ? 1 : 0;
        std::copy( addr_peripheral.begin(), addr_peripheral.end(), &m4_m3[ 15 ] );
        m4_m3[ 14 ] = 0x80;

        const std::uint8_t* m3 = &m4_m3[ 16 ];
        const std::uint8_t* m4 = &m4_m3[ 0 ];

        auto t0 = aes_le( key, n1 );
        auto t1 = aes_le( key, xor_( t0, n2 ) );
        auto t2 = aes_le( key, xor_( t1, r ) );
        auto t3 = aes_le( key, xor_( t2, m3 ) );

        return aes_le( key, xor_( t3, xor_( aes_cmac_k2_subkey_generation( key ), m4 ) ) );
    }

    std::uint32_t nrf52_security_tool_box::g2(
        const std::uint8_t*                 u,
        const std::uint8_t*                 v,
        const bluetoe::details::uint128_t&  x,
        const bluetoe::details::uint128_t&  y )
    {
        auto t0 = aes_le( x, u + 16 );
        auto t1 = aes_le( x, xor_( t0, u ) );
        auto t2 = aes_le( x, xor_( t1, v + 16 ) );
        auto t3 = aes_le( x, xor_( t2, v ) );
        auto t4 = aes_le( x, xor_( t3, xor_( aes_cmac_k1_subkey_generation( x ), y ) ) );

        return bluetoe::details::read_32bit( t4.begin() );
    }

    bluetoe::details::uint128_t nrf52_security_tool_box::create_passkey()
    {
        const bluetoe::details::uint128_t result{{
            random_number8(), random_number8(), random_number8()
        }};

        return result;
    }

    //////////////////////////////////////////
    // class radio_hardware_without_crypto_support
    void radio_hardware_without_crypto_support::init( void (*isr)( void* ), void* that )
    {
        start_high_frequency_clock();

        init_debug();
        init_radio( false );
        init_timer();

        enable_interrupts( isr, that );
    }

    void radio_hardware_without_crypto_support::configure_radio_channel( unsigned channel )
    {
        // TODO Why do we need that?
        // Maybe: When the radio is ramping up for transmitting or reception and is then disabled,
        //        the next state would be TXDISABLE or RXDISABLE
        while ((nrf_radio->STATE & RADIO_STATE_STATE_Msk) != RADIO_STATE_STATE_Disabled);

        nrf_radio->FREQUENCY   = frequency_from_channel( channel );
        nrf_radio->DATAWHITEIV = channel & 0x3F;

        nrf_radio->INTENCLR    = 0xffffffff;
        nrf_timer->INTENCLR    = 0xffffffff;
    }

    void radio_hardware_without_crypto_support::configure_transmit_train(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_radio->SHORTS      =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_RXEN_Msk
          | RADIO_SHORTS_ADDRESS_BCSTART_Msk;

        nrf_ppi->CHENCLR = all_preprogramed_ppi_channels_mask;
        nrf_ppi->CHENSET =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        nrf_radio->EVENTS_PAYLOAD = 0;
        nrf_radio->EVENTS_DISABLED = 0;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( transmit_data.size << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_without_crypto_support::configure_final_transmit(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_radio->SHORTS =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk;

        nrf_ppi->CHENCLR  = all_preprogramed_ppi_channels_mask;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( transmit_data.size << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_without_crypto_support::configure_receive_train(
        const bluetoe::link_layer::read_buffer& receive_buffer )
    {
        nrf_radio->SHORTS      =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_TXEN_Msk;

        nrf_ppi->CHENCLR = all_preprogramed_ppi_channels_mask;
        nrf_ppi->CHENSET       =
              ( 1 << compare0_rxen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 1 ] = 0;
        nrf_radio->EVENTS_PAYLOAD = 0;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( receive_buffer.buffer );
        nrf_radio->PCNF1       = ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk ) | ( receive_buffer.size << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_without_crypto_support::stop_radio()
    {
        nrf_radio->INTENCLR      = RADIO_INTENSET_DISABLED_Msk;
        nrf_radio->SHORTS        = 0;
        nrf_radio->TASKS_DISABLE = 1;
    }

    void radio_hardware_without_crypto_support::capture_timer_anchor()
    {
        nrf_timer->TASKS_CAPTURE[ 2 ] = 1;
    }

    void radio_hardware_without_crypto_support::store_timer_anchor( int offset_us )
    {
        anchor_offset_ = link_layer::delta_time( nrf_timer->CC[ 2 ] + offset_us );
    }

    std::pair< bool, bool > radio_hardware_without_crypto_support::received_pdu()
    {
        const bool result = ( nrf_radio->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) == RADIO_CRCSTATUS_CRCSTATUS_CRCOk && nrf_radio->EVENTS_PAYLOAD;
        nrf_radio->EVENTS_PAYLOAD = 0;

        return { result, result };
    }

    std::uint32_t radio_hardware_without_crypto_support::now()
    {
        nrf_timer->TASKS_CAPTURE[ 3 ] = 1;
        return nrf_timer->CC[ 3 ] - anchor_offset_.usec();
    }

    void radio_hardware_without_crypto_support::schedule_advertisment_event(
        bluetoe::link_layer::delta_time when,
        std::uint32_t                   read_timeout_us )
    {
        nrf_radio->INTENSET    = RADIO_INTENSET_DISABLED_Msk;

        if ( when.zero() )
        {
            nrf_timer->TASKS_CAPTURE[ 1 ]  = 1;
            nrf_timer->CC[ 1 ]            += us_radio_tx_startup_time + read_timeout_us;

            nrf_radio->TASKS_TXEN          = 1;
        }
        else
        {
            nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
            nrf_timer->CC[ 0 ]             = when.usec() - us_radio_tx_startup_time + anchor_offset_.usec();
            nrf_timer->CC[ 1 ]             = nrf_timer->CC[ 0 ] + us_radio_tx_startup_time + read_timeout_us;
            nrf_timer->CC[ 2 ]             = 0;

            // manually triggering event for timer beeing already behind target time
            nrf_timer->TASKS_CAPTURE[ 3 ]  = 1;

            if ( nrf_timer->EVENTS_COMPARE[ 0 ] || nrf_timer->CC[ 3 ] >= nrf_timer->CC[ 0 ] )
            {
                nrf_timer->TASKS_CLEAR = 1;
                nrf_radio->TASKS_TXEN = 1;
            }

            anchor_offset_ += when;
        }
    }

    void radio_hardware_without_crypto_support::schedule_reception(
        std::uint32_t                   begin_us,
        std::uint32_t                   end_us )
    {
        nrf_radio->EVENTS_DISABLED = 0;
        nrf_radio->INTENSET    = RADIO_INTENSET_DISABLED_Msk;

        nrf_timer->CC[ 0 ] = begin_us + anchor_offset_.usec();
        nrf_timer->CC[ 1 ] = end_us + anchor_offset_.usec();
    }

    void radio_hardware_without_crypto_support::stop_timeout_timer()
    {
        nrf_ppi->CHENCLR = ( 1 << compare1_disable_ppi_channel );
    }

    std::uint32_t radio_hardware_without_crypto_support::static_random_address_seed()
    {
        return NRF_FICR->DEVICEID[ 0 ];
    }

    void radio_hardware_without_crypto_support::set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
    {
        nrf_radio->BASE0     = ( access_address << 8 ) & 0xFFFFFF00;
        nrf_radio->PREFIX0   = ( access_address >> 24 ) & RADIO_PREFIX0_AP0_Msk;
        nrf_radio->CRCINIT   = crc_init;
    }

    void radio_hardware_without_crypto_support::debug_toggle()
    {
        toggle_debug_pin();
    }

    bluetoe::link_layer::delta_time radio_hardware_without_crypto_support::anchor_offset_;

    //////////////////////////////////////////////
    // class radio_hardware_with_crypto_support
    static constexpr std::size_t ccm_key_offset = 0;
    static constexpr std::size_t ccm_packet_counter_offset = 16;
    static constexpr std::size_t ccm_packet_counter_size   = 5;
    static constexpr std::size_t ccm_direction_offset = 24;
    static constexpr std::size_t ccm_iv_offset  = 25;

    static constexpr std::uint8_t master_to_slave_ccm_direction = 0x01;
    static constexpr std::uint8_t slave_to_master_ccm_direction = 0x00;

    // the value MAXPACKETSIZE from the documentation seems to be the maximum value, the size field can store,
    // and is independent from the MTU size (https://devzone.nordicsemi.com/f/nordic-q-a/13123/what-is-actual-size-required-for-scratch-area-for-ccm-on-nrf52/50031#50031)
    static constexpr std::size_t scratch_size   = 267;

    static struct alignas( 4 ) ccm_data_struct_t {
        std::uint8_t data[ 33 ];
    } ccm_data_struct;

    static struct alignas( 4 ) scratch_area_t {
        std::uint8_t data[ scratch_size ];
    } scratch_area;

    static void setup_ccm_data_structure( const bluetoe::details::uint128_t& key, std::uint64_t IV )
    {
        std::copy( key.rbegin(), key.rend(), &ccm_data_struct.data[ ccm_key_offset ] );
        std::fill( &ccm_data_struct.data[ ccm_packet_counter_offset ],
            &ccm_data_struct.data[ ccm_packet_counter_offset + ccm_packet_counter_size ], 0 );

        details::write_64bit( &ccm_data_struct.data[ ccm_iv_offset ], IV );
    }

    void radio_hardware_with_crypto_support::init( std::uint8_t* encrypted_area, void (*isr)( void* ), void* that )
    {
        encrypted_area_ = encrypted_area;
        start_high_frequency_clock();

        init_debug();
        init_radio( true );
        init_timer();

        enable_interrupts( isr, that );

        nrf_random->CONFIG  = RNG_CONFIG_DERCEN_Msk;
        nrf_random->SHORTS  = RNG_SHORTS_VALRDY_STOP_Msk;

        nrf_ccm->SCRATCHPTR = reinterpret_cast< std::uintptr_t >( &scratch_area );
        nrf_ccm->CNFPTR     = reinterpret_cast< std::uintptr_t >( &ccm_data_struct );

        configure_encryption( false, false );
    }

    void radio_hardware_with_crypto_support::configure_transmit_train(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_radio->SHORTS      =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_RXEN_Msk
          | RADIO_SHORTS_ADDRESS_BCSTART_Msk;

        nrf_ppi->CHENCLR = all_preprogramed_ppi_channels_mask;
        nrf_ppi->CHENSET =
              ( 1 << compare0_txen_ppi_channel )
            | ( 1 << compare1_disable_ppi_channel )
            | ( 1 << radio_end_capture2_ppi_channel );

        nrf_radio->EVENTS_PAYLOAD = 0;
        nrf_radio->EVENTS_DISABLED = 0;

        nrf_radio->PACKETPTR   = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
        nrf_radio->PCNF1       =
            ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk )
          | ( ( transmit_data.size - 1 ) << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_with_crypto_support::configure_final_transmit(
        const bluetoe::link_layer::write_buffer&    transmit_data )
    {
        nrf_ppi->CHENCLR        = all_preprogramed_ppi_channels_mask;

        nrf_radio->SHORTS =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk;

        // only encrypt none empty PDUs
        if ( transmit_encrypted_ && transmit_data.buffer[ 1 ] != 0 )
        {
            transmit_counter_.copy_to( &ccm_data_struct.data[ ccm_packet_counter_offset ] );
            ccm_data_struct.data[ ccm_direction_offset ] = slave_to_master_ccm_direction;

            nrf_ccm->SHORTS  = CCM_SHORTS_ENDKSGEN_CRYPT_Msk;
            nrf_ccm->MODE    =
                  ( CCM_MODE_MODE_Encryption << CCM_MODE_MODE_Pos )
                | ( CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos );
            nrf_ccm->OUTPTR     = reinterpret_cast< std::uint32_t >( encrypted_area_ );
            nrf_ccm->INPTR      = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
            nrf_ccm->SCRATCHPTR = reinterpret_cast< std::uintptr_t >( &scratch_area );

            nrf_ccm->EVENTS_ENDKSGEN    = 0;
            nrf_ccm->EVENTS_ENDCRYPT    = 0;

            nrf_radio->PACKETPTR = reinterpret_cast< std::uint32_t >( encrypted_area_ );
            nrf_radio->PCNF1 =
                ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk )
              | ( ( transmit_data.size + encryption_mic_size - 1 ) << RADIO_PCNF1_MAXLEN_Pos );

            nrf_ccm->TASKS_KSGEN        = 1;
        }
        else
        {
            nrf_radio->PACKETPTR    = reinterpret_cast< std::uint32_t >( transmit_data.buffer );
            nrf_radio->PCNF1        =
                ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk )
              | ( ( transmit_data.size - 1 ) << RADIO_PCNF1_MAXLEN_Pos );
        }
    }

    void radio_hardware_with_crypto_support::configure_receive_train(
        const bluetoe::link_layer::read_buffer&     receive_buffer )
    {
        if ( receive_encrypted_ )
        {
            receive_counter_.copy_to( &ccm_data_struct.data[ ccm_packet_counter_offset ] );
            ccm_data_struct.data[ ccm_direction_offset ] = master_to_slave_ccm_direction;

            // Reseting the CCM before every connection event seems to workaround a bug that
            // Causes the CCM to start decrypting an incomming PDU, before it was actually received
            // which causes overwriting the receive buffer, if the length field in the encrypted_area_
            // was long enough.
            // (https://devzone.nordicsemi.com/f/nordic-q-a/43656/what-causes-decryption-before-receiving)
            nrf_ccm->ENABLE = CCM_ENABLE_ENABLE_Disabled;
            nrf_ccm->ENABLE = CCM_ENABLE_ENABLE_Enabled;
            nrf_ccm->MODE   =
                  ( CCM_MODE_MODE_Decryption << CCM_MODE_MODE_Pos )
                | ( CCM_MODE_LENGTH_Extended << CCM_MODE_LENGTH_Pos );
            nrf_ccm->CNFPTR     = reinterpret_cast< std::uintptr_t >( &ccm_data_struct );
            nrf_ccm->INPTR      = reinterpret_cast< std::uint32_t >( encrypted_area_ );
            nrf_ccm->OUTPTR     = reinterpret_cast< std::uint32_t >( receive_buffer.buffer );
            nrf_ccm->SCRATCHPTR = reinterpret_cast< std::uintptr_t >( &scratch_area );
            nrf_ccm->SHORTS     = 0;
            nrf_ccm->EVENTS_ENDKSGEN = 0;
            nrf_ccm->EVENTS_ENDCRYPT = 0;
            nrf_ccm->EVENTS_ERROR = 0;

            nrf_ccm->TASKS_KSGEN = 1;
        }

        static constexpr std::uint32_t radio_shorts =
            RADIO_SHORTS_READY_START_Msk
          | RADIO_SHORTS_END_DISABLE_Msk
          | RADIO_SHORTS_DISABLED_TXEN_Msk;

        static constexpr std::uint32_t encrypted_ppi_channels =
            ( 1 << radio_address_ccm_crypt )
          | ( 1 << compare0_rxen_ppi_channel )
          | ( 1 << compare1_disable_ppi_channel )
          | ( 1 << radio_end_capture2_ppi_channel );

        static constexpr std::uint32_t unencrypted_ppi_channels =
            ( 1 << compare0_rxen_ppi_channel )
          | ( 1 << compare1_disable_ppi_channel )
          | ( 1 << radio_end_capture2_ppi_channel );

        nrf_radio->SHORTS   = radio_shorts;
        nrf_ppi->CHENCLR    = all_preprogramed_ppi_channels_mask;
        nrf_ppi->CHENSET    = receive_encrypted_
            ? encrypted_ppi_channels
            : unencrypted_ppi_channels;

        nrf_timer->EVENTS_COMPARE[ 0 ] = 0;
        nrf_timer->EVENTS_COMPARE[ 1 ] = 0;
        nrf_radio->EVENTS_PAYLOAD = 0;

        const auto mic_size = []( bool receive_encrypted ){
            return receive_encrypted
                ? encryption_mic_size
                : 0;
        };

        nrf_radio->PACKETPTR   = receive_encrypted_
            ? reinterpret_cast< std::uint32_t >( encrypted_area_ )
            : reinterpret_cast< std::uint32_t >( receive_buffer.buffer );

        nrf_radio->PCNF1       =
            ( nrf_radio->PCNF1 & ~RADIO_PCNF1_MAXLEN_Msk )
          | ( ( receive_buffer.size + mic_size( receive_encrypted_ ) ) << RADIO_PCNF1_MAXLEN_Pos );
    }

    void radio_hardware_with_crypto_support::store_timer_anchor( int offset_us )
    {
        // TODO: Hack is required, as we capture the Anchor at the END of the PDU
        // Issue: #76 Taking Anchor from End of PDU
        if ( receive_encrypted_ && offset_us < -80 )
            offset_us -= encryption_mic_size * 8;

        radio_hardware_without_crypto_support::store_timer_anchor( offset_us );
    }

    std::pair< bool, bool > radio_hardware_with_crypto_support::received_pdu()
    {
        const auto receive_size = [](){
            return reinterpret_cast< const std::uint8_t* >( nrf_radio->PACKETPTR )[ 1 ];
        };

        const bool timeout      = ( nrf_radio->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) != RADIO_CRCSTATUS_CRCSTATUS_CRCOk || !nrf_radio->EVENTS_PAYLOAD;
        const bool crc_error    = !timeout && ( nrf_radio->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk ) != RADIO_CRCSTATUS_CRCSTATUS_CRCOk;
        const bool mic_error    = receive_encrypted_ && receive_size() != 0 && ( nrf_ccm->MICSTATUS & CCM_MICSTATUS_MICSTATUS_Msk ) == CCM_MICSTATUS_MICSTATUS_CheckFailed;
        const bool bus_error    = receive_encrypted_ && nrf_ccm->EVENTS_ERROR;
        const bool not_decrypt  = receive_encrypted_ && receive_size() != 0 && nrf_ccm->EVENTS_ENDCRYPT == 0;

        const bool valid_anchor = !timeout && !bus_error;
        const bool valid_pdu    = valid_anchor && !crc_error && !not_decrypt && !mic_error;

        nrf_radio->EVENTS_PAYLOAD = 0;

        return { valid_anchor, valid_pdu };
    }

    void radio_hardware_with_crypto_support::configure_encryption( bool receive, bool transmit )
    {
        if ( receive && transmit )
            transmit_counter_ = counter();

        if ( receive && !transmit )
            receive_counter_ = counter();

        if ( !receive && !transmit )
        {
            // delete key out of memory
            std::memset( &ccm_data_struct, 0, sizeof( ccm_data_struct ) );
            nrf_ccm->ENABLE = nrf_ccm->ENABLE & ~CCM_ENABLE_ENABLE_Msk;
        }

        receive_encrypted_ = receive;
        transmit_encrypted_ = transmit;
    }

    std::pair< std::uint64_t, std::uint32_t > radio_hardware_with_crypto_support::setup_encryption( bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm )
    {
        const std::uint64_t skds = random_number64();
        const std::uint32_t ivs  = random_number32();

        bluetoe::details::uint128_t session_descriminator;
        details::write_64bit( &session_descriminator[ 0 ], skdm );
        details::write_64bit( &session_descriminator[ 8 ], skds );

        setup_ccm_data_structure(
            aes_le( key, session_descriminator ),
            static_cast< std::uint64_t >( ivm ) | ( static_cast< std::uint64_t >( ivs ) << 32 ) );

        return { skds, ivs };
    }

    void radio_hardware_with_crypto_support::setup_identity_resolving_address(
        const std::uint8_t* address )
    {
        if ( identity_resolving_enabled_ )
        {
            nrf_aar->EVENTS_END         = 0;
            nrf_aar->EVENTS_RESOLVED    = 0;
            nrf_aar->EVENTS_NOTRESOLVED = 0;

            nrf_aar->ADDRPTR = reinterpret_cast< std::uint32_t >( address ) ;
        }
    }

    void radio_hardware_with_crypto_support::set_identity_resolving_key(
        const details::identity_resolving_key_t& irk )
    {
        static details::identity_resolving_key_t irk_storage;
        irk_storage = irk;

        identity_resolving_enabled_ = true;

        // disable CCM
        nrf_ccm->ENABLE   = CCM_ENABLE_ENABLE_Disabled;
        nrf_ccm->INTENCLR = 0xFFFFFFFF;
        nrf_ppi->CHENCLR  = ( 1 << radio_address_ccm_crypt );

        // setup AAR
        nrf_ppi->CHENSET    = ( 1 << radio_bcmatch_aar_start_channel );
        nrf_radio->BCC      = 16 + 2 * ( 6 * 8 );
        nrf_aar->SCRATCHPTR = reinterpret_cast< std::uintptr_t >( &scratch_area );
        nrf_aar->IRKPTR     = reinterpret_cast< std::uint32_t >( &irk_storage );
        nrf_aar->NIRK       = 1;

        nrf_aar->ENABLE     = AAR_ENABLE_ENABLE_Msk;
    }

    bool radio_hardware_with_crypto_support::resolving_address_invalid()
    {
        if ( identity_resolving_enabled_ )
        {
            while ( !nrf_aar->EVENTS_END )
                ;

            if ( nrf_aar->EVENTS_NOTRESOLVED )
                return true;
        }

        return false;
    }

    volatile bool radio_hardware_with_crypto_support::receive_encrypted_  = false;
    volatile bool radio_hardware_with_crypto_support::transmit_encrypted_ = false;
    std::uint8_t* radio_hardware_with_crypto_support::encrypted_area_;
    counter radio_hardware_with_crypto_support::transmit_counter_;
    counter radio_hardware_with_crypto_support::receive_counter_;
    bool    radio_hardware_with_crypto_support::identity_resolving_enabled_ = false;

} // namespace nrf52_details
} // namespace bluetoe

extern "C" void RADIO_IRQHandler(void)
{
    using namespace bluetoe::nrf52_details;
    set_isr_pin();

    assert( bluetoe::nrf::nrf_radio->EVENTS_DISABLED );

    assert( bluetoe::nrf52_details::instance );
    assert( bluetoe::nrf52_details::isr_handler );

    isr_handler( bluetoe::nrf52_details::instance );

    bluetoe::nrf::nrf_radio->EVENTS_END       = 0;
    bluetoe::nrf::nrf_radio->EVENTS_DISABLED  = 0;
    bluetoe::nrf::nrf_radio->EVENTS_READY     = 0;
    bluetoe::nrf::nrf_radio->EVENTS_ADDRESS   = 0;
    bluetoe::nrf::nrf_radio->EVENTS_PAYLOAD   = 0;

    reset_isr_pin();
}
