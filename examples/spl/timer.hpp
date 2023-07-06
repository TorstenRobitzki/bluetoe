#ifndef SPL_TIMER_HPP
#define SPL_TIMER_HPP

namespace spl {

    template < typename ... Options >
    class timer
    {
    public:
        bool handle_event();
    };
}

#endif
