#ifndef SOURCE_ASSERT_ASSERT_H
#define SOURCE_ASSERT_ASSERT_H

#include <cstdint>

#ifdef USING_ASSERT_HASH
extern "C++" {
#   include "assert_hash/assert_hash.hpp"
}
#endif

#undef assert

using assert_hash_type = std::uint32_t;

#ifdef NDEBUG           /* required by ANSI standard */
#   define assert(__e) ((void)0)
#else
#   ifdef USING_ASSERT_HASH
#       define assert(__e) ((__e) ? (void)0 : __assert_hash_func ( assert_hash::file_and_line_hash< assert_hash_type >( __FILE__, __LINE__ )) )
#   else
#       define assert(__e) ((__e) ? (void)0 : __assert_hash_func ( 0 ) )
#   endif /* USING_ASSERT_HASH */
#endif /* !NDEBUG */

extern "C" void __assert_hash_func(assert_hash_type) __attribute__((__noreturn__));

#endif
