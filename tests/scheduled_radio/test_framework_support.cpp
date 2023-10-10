#include "test_framework_support.hpp"

#include "system_nrf52.h"

extern "C" void _close(void) {}
extern "C" void _fstat(void) {}
extern "C" void _getpid(void) {}
extern "C" void _gettimeofday(void) {}
extern "C" void _isatty(void) {}
extern "C" void _kill(void) {}
extern "C" void _lseek(void) {}
extern "C" void _open(void) {}
extern "C" void _read(void) {}
extern "C" void _write(void) {}

std::ostream& Catch::cout()
{
    return *static_cast< std::ostream* >( 0 );
}

std::ostream& Catch::cerr()
{
    return *static_cast< std::ostream* >( 0 );
}

std::ostream& Catch::clog()
{
    return *static_cast< std::ostream* >( 0 );
}
