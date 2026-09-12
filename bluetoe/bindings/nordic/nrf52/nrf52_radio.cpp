#include <bluetoe/nrf52_radio.hpp>

#include <bluetoe/nrf.hpp>

namespace bluetoe
{
    namespace nrf52_details
    {
        radio_base::radio_base()
        {
            // bias correction on, one value per start; the toolbox starts it per value it draws
            nrf::nrf_random->CONFIG = RNG_CONFIG_DERCEN_Msk;
            nrf::nrf_random->SHORTS = RNG_SHORTS_VALRDY_STOP_Msk;
        }

        void radio_base::run()
        {
            __WFE();
        }

        void radio_base::wake_up()
        {
            __SEV();
        }
    }
}
