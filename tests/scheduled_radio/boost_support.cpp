#include "boost_support.hpp"

extern "C" void _close(void) {}
extern "C" void _fstat(void) {}
extern "C" void _getpid(void) {}
extern "C" void _gettimeofday(void) {}
extern "C" void _isatty(void) {}
extern "C" void _kill(void) {}
extern "C" void _lseek(void) {}
extern "C" void _open(void) {}
extern "C" void _read(void) {}
extern "C" void _times(void) {}
extern "C" void _write(void) {}

extern "C" int sigaction(int, void*, void*)
{
    return -1;
}

extern "C" int sigaltstack(const void*, void*)
{
    return -1;
}
