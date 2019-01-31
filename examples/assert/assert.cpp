#include <assert.h>

extern "C" void HardFault_Handler() __attribute__ ((noreturn));

extern "C" void __assert_hash_func( assert_hash_type hash )
{
    HardFault_Handler();
}
