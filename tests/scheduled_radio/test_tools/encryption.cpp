#include "test_tools/encryption.hpp"

#include "aes.h"

#include <algorithm>
#include <cassert>

namespace bluetoe {
namespace test_rig {

    namespace {

        constexpr std::size_t   block_size  = 16;
        constexpr std::size_t   mic_size    = 4;

        // the CCM's parameters: a MIC of 4 bytes and a length field of 2, with additional data
        constexpr std::uint8_t  b0_flags    = 0x49;
        constexpr std::uint8_t  ctr_flags   = 0x01;

        // the header bits the MIC covers: the LLID and the reserved bits, not NESN, SN and MD
        constexpr std::uint8_t  header_mask = 0xe3;

        std::array< std::uint8_t, 16 > reversed( const bluetoe::details::uint128_t& value )
        {
            std::array< std::uint8_t, 16 > result;
            std::reverse_copy( value.begin(), value.end(), result.begin() );

            return result;
        }

        void write_big_endian( std::uint8_t* out, std::uint64_t value, std::size_t bytes )
        {
            for ( std::size_t i = 0; i != bytes; ++i )
                out[ i ] = static_cast< std::uint8_t >( value >> ( 8 * ( bytes - 1 - i ) ) );
        }
    }

    /*
     * The session key is e( LTK, SKD ) with SKD = SKDm || SKDs, SKDm the less significant
     * half; e() takes its key and its block most significant byte first. The IV is
     * IVm || IVs the same way, and enters the nonce least significant octet first.
     */
    encryption_session::encryption_session(
        const bluetoe::details::uint128_t& long_term_key,
        std::uint64_t skdm, std::uint64_t skds, std::uint32_t ivm, std::uint32_t ivs )
    {
        std::array< std::uint8_t, 16 > diversifier;
        write_big_endian( &diversifier[ 0 ], skds, 8 );
        write_big_endian( &diversifier[ 8 ], skdm, 8 );

        AES_ctx context;
        AES_init_ctx( &context, reversed( long_term_key ).data() );
        AES_ECB_encrypt( &context, diversifier.data() );

        key_ = diversifier;

        for ( std::size_t i = 0; i != 4; ++i )
        {
            iv_[ i ]     = static_cast< std::uint8_t >( ivm >> ( 8 * i ) );
            iv_[ 4 + i ] = static_cast< std::uint8_t >( ivs >> ( 8 * i ) );
        }
    }

    const std::array< std::uint8_t, 16 >& encryption_session::key() const
    {
        return key_;
    }

    /*
     * The nonce: the 39 bit packet counter least significant octet first, the direction bit
     * as the most significant bit of its last octet, then the IV.
     */
    std::array< std::uint8_t, 13 > encryption_session::nonce( encryption_direction direction, std::uint64_t packet_counter ) const
    {
        assert( packet_counter < ( std::uint64_t( 1 ) << 39 ) );

        std::array< std::uint8_t, 13 > result;

        for ( std::size_t i = 0; i != 5; ++i )
            result[ i ] = static_cast< std::uint8_t >( packet_counter >> ( 8 * i ) );

        if ( direction == encryption_direction::central_to_peripheral )
            result[ 4 ] |= 0x80;

        std::copy( iv_.begin(), iv_.end(), result.begin() + 5 );

        return result;
    }

    std::array< std::uint8_t, 16 > encryption_session::encrypt_block( const std::array< std::uint8_t, 16 >& block ) const
    {
        std::array< std::uint8_t, 16 > result = block;

        AES_ctx context;
        AES_init_ctx( &context, key_.data() );
        AES_ECB_encrypt( &context, result.data() );

        return result;
    }

    /*
     * CBC-MAC over B0, the block with the nonce and the length of the plaintext, then the
     * block with the additional data, the masked header byte behind its two byte length, then
     * the plaintext in blocks padded with zeros; the tag is the first four bytes of the last
     * result, masked with the first key stream block.
     */
    std::array< std::uint8_t, 4 > encryption_session::mic(
        const std::array< std::uint8_t, 13 >& nonce, llid kind, std::span< const std::uint8_t > plaintext ) const
    {
        std::array< std::uint8_t, 16 > block = {};
        block[ 0 ] = b0_flags;
        std::copy( nonce.begin(), nonce.end(), block.begin() + 1 );
        write_big_endian( &block[ 14 ], plaintext.size(), 2 );

        std::array< std::uint8_t, 16 > x = encrypt_block( block );

        block = {};
        block[ 1 ] = 1;
        block[ 2 ] = static_cast< std::uint8_t >( kind ) & header_mask;

        for ( std::size_t i = 0; i != block_size; ++i )
            block[ i ] ^= x[ i ];

        x = encrypt_block( block );

        for ( std::size_t offset = 0; offset < plaintext.size(); offset += block_size )
        {
            block = x;

            for ( std::size_t i = 0; i != block_size && offset + i < plaintext.size(); ++i )
                block[ i ] ^= plaintext[ offset + i ];

            x = encrypt_block( block );
        }

        const std::vector< std::uint8_t > s0 = key_stream( nonce, 0 );

        std::array< std::uint8_t, 4 > result;

        for ( std::size_t i = 0; i != mic_size; ++i )
            result[ i ] = x[ i ] ^ s0[ i ];

        return result;
    }

    /*
     * The key stream: the counter blocks A0, A1, ... encrypted; A0 masks the tag, the rest
     * the payload. `size` bytes of it after A0, rounded up to whole blocks.
     */
    std::vector< std::uint8_t > encryption_session::key_stream( const std::array< std::uint8_t, 13 >& nonce, std::size_t size ) const
    {
        std::vector< std::uint8_t > result;

        for ( std::uint16_t counter = 0; counter * block_size < size + block_size; ++counter )
        {
            std::array< std::uint8_t, 16 > block = {};
            block[ 0 ] = ctr_flags;
            std::copy( nonce.begin(), nonce.end(), block.begin() + 1 );
            write_big_endian( &block[ 14 ], counter, 2 );

            const std::array< std::uint8_t, 16 > s = encrypt_block( block );
            result.insert( result.end(), s.begin(), s.end() );
        }

        return result;
    }

    std::vector< std::uint8_t > encryption_session::encrypt(
        encryption_direction direction, std::uint64_t packet_counter, llid kind, std::span< const std::uint8_t > payload ) const
    {
        const auto n      = nonce( direction, packet_counter );
        const auto stream = key_stream( n, payload.size() );
        const auto tag    = mic( n, kind, payload );

        std::vector< std::uint8_t > result( payload.begin(), payload.end() );

        for ( std::size_t i = 0; i != result.size(); ++i )
            result[ i ] ^= stream[ block_size + i ];

        result.insert( result.end(), tag.begin(), tag.end() );

        return result;
    }

    std::optional< std::vector< std::uint8_t > > encryption_session::decrypt(
        encryption_direction direction, std::uint64_t packet_counter, llid kind, std::span< const std::uint8_t > ciphertext ) const
    {
        if ( ciphertext.size() < mic_size )
            return std::nullopt;

        const auto n      = nonce( direction, packet_counter );
        const auto stream = key_stream( n, ciphertext.size() - mic_size );

        std::vector< std::uint8_t > result( ciphertext.begin(), ciphertext.end() - mic_size );

        for ( std::size_t i = 0; i != result.size(); ++i )
            result[ i ] ^= stream[ block_size + i ];

        const auto tag = mic( n, kind, result );

        if ( !std::equal( tag.begin(), tag.end(), ciphertext.end() - mic_size ) )
            return std::nullopt;

        return result;
    }
}
}
