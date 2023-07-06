#ifndef SPL_TEMPERATURE_HPP
#define SPL_TEMPERATURE_HPP

#include <cstdint>

namespace spl {

    namespace details {
        class nrf52_temperature_base
        {
        public:
            static void init();
            static std::uint32_t value();
        };
    }

    /**
     * @brief abstraction of a temperature sensing device
     *
     * Potential options could be resolution, bus to connect the sensor and so on...
     */
    template < typename ... Options >
    class temperature : private details::nrf52_temperature_base
    {
    public:
        temperature();

        std::uint32_t value();

        bool handle_event();
    };


    // implementation
    template < typename ... Options >
    temperature< Options... >::temperature()
    {
        init();
    }

    template < typename ... Options >
    std::uint32_t temperature< Options... >::value()
    {
        return nrf52_temperature_base::value();
    }

    template < typename ... Options >
    bool temperature< Options... >::handle_event()
    {
        return false;
    }
}

#endif
