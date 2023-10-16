#include "test_framework_support.hpp"

#include "system_nrf52.h"
#include <cassert>

extern "C" void _close(void) { assert( false ); }
extern "C" void _fstat(void) { assert( false ); }
extern "C" void _getpid(void) { assert( false ); }
extern "C" void _gettimeofday(void) { assert( false ); }
extern "C" void _isatty(void) { assert( false ); }
extern "C" void _kill(void) { assert( false ); }
extern "C" void _lseek(void) { assert( false ); }
extern "C" void _open(void) { assert( false ); }
extern "C" void _read(void) { assert( false ); }
extern "C" void _write(void) { assert( false ); }

std::ostream& Catch::cout()
{
     assert( false );
    return *static_cast< std::ostream* >( 0 );
}

std::ostream& Catch::cerr()
{
     assert( false );
    return *static_cast< std::ostream* >( 0 );
}

std::ostream& Catch::clog()
{
     assert( false );
    return *static_cast< std::ostream* >( 0 );
}
