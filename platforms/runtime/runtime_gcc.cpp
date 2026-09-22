#include <cstddef>
#include <cstdint>

extern "C" void __cxa_pure_virtual(void) {}

extern "C" void (*__preinit_array_start []) (void) __attribute__((weak));
extern "C" void (*__preinit_array_end []) (void) __attribute__((weak));
extern "C" void (*__init_array_start []) (void) __attribute__((weak));
extern "C" void (*__init_array_end []) (void) __attribute__((weak));
extern "C" void (*__fini_array_start []) (void) __attribute__((weak));
extern "C" void (*__fini_array_end []) (void) __attribute__((weak));

extern "C" typedef void (*vector)();

extern "C" std::uint32_t __bss_start__;
extern "C" std::uint32_t __bss_end__;

extern "C" void _start(void) {
    extern int main(void);

    // zero initialize static allocated variables
    for ( std::uint32_t* bss = &__bss_start__; bss != &__bss_end__; ++bss )
        *bss = 0;

    for ( vector* init = &__preinit_array_start[ 0 ]; init != &__preinit_array_end[ 0 ]; ++init )
        (*init)();

    for ( vector* init = &__init_array_start[ 0 ]; init != &__init_array_end[ 0 ]; ++init )
        (*init)();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
// ISO C++ forbids calling main. We are silencing this warning explicitly.
// This startup code replaces the one provided by newlib, which is not being used in
// order to save space from the final binary.
    main();
#pragma GCC diagnostic pop

    for ( vector* finit = &__fini_array_start[ 0 ]; finit != &__fini_array_end[ 0 ]; ++finit )
        (*finit)();

}

/*
 * The memory functions the compiler emits calls to, in place of newlib's: newlib's are
 * unrolled, half a kilobyte for the three of them. These copy a word at a time while both
 * sides are aligned, which the copies of whole objects are, and bytes otherwise; a byte loop
 * alone was too slow for the tester, which copies a PDU inside the inter frame space. The
 * attribute keeps the compiler from recognising the loops and turning them back into calls
 * to the very function they implement, and lets the word access alias the bytes.
 */
#define BLUETOE_MEMORY_FUNCTION __attribute__(( optimize( "no-tree-loop-distribute-patterns", "no-strict-aliasing" ) ))

namespace {
    bool word_aligned( const void* first, const void* second )
    {
        return ( ( reinterpret_cast< std::uintptr_t >( first ) | reinterpret_cast< std::uintptr_t >( second ) ) & 3 ) == 0;
    }
}

extern "C" BLUETOE_MEMORY_FUNCTION void* memcpy( void* destination, const void* source, std::size_t size )
{
    std::uint8_t*       to   = static_cast< std::uint8_t* >( destination );
    const std::uint8_t* from = static_cast< const std::uint8_t* >( source );

    if ( word_aligned( to, from ) )
    {
        for ( ; size >= 4; size -= 4, to += 4, from += 4 )
            *reinterpret_cast< std::uint32_t* >( to ) = *reinterpret_cast< const std::uint32_t* >( from );
    }

    for ( ; size; --size )
        *to++ = *from++;

    return destination;
}

extern "C" BLUETOE_MEMORY_FUNCTION void* memmove( void* destination, const void* source, std::size_t size )
{
    std::uint8_t*       to   = static_cast< std::uint8_t* >( destination );
    const std::uint8_t* from = static_cast< const std::uint8_t* >( source );

    // forward is safe unless the destination starts inside the source
    if ( to <= from )
        return memcpy( destination, source, size );

    to   += size;
    from += size;

    if ( word_aligned( to, from ) )
    {
        for ( ; size >= 4; size -= 4 )
        {
            to   -= 4;
            from -= 4;
            *reinterpret_cast< std::uint32_t* >( to ) = *reinterpret_cast< const std::uint32_t* >( from );
        }
    }

    for ( ; size; --size )
        *--to = *--from;

    return destination;
}

extern "C" BLUETOE_MEMORY_FUNCTION void* memset( void* destination, int value, std::size_t size )
{
    std::uint8_t* to = static_cast< std::uint8_t* >( destination );

    for ( ; size; --size )
        *to++ = static_cast< std::uint8_t >( value );

    return destination;
}

extern "C" int memcmp( const void* first, const void* second, std::size_t size )
{
    const std::uint8_t* a = static_cast< const std::uint8_t* >( first );
    const std::uint8_t* b = static_cast< const std::uint8_t* >( second );

    for ( ; size; --size, ++a, ++b )
    {
        if ( *a != *b )
            return *a < *b ? -1 : 1;
    }

    return 0;
}
