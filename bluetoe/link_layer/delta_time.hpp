#ifndef BLUETOE_LINK_LAYER_DELTA_TIME_HPP
#define BLUETOE_LINK_LAYER_DELTA_TIME_HPP

#include <cstdint>

namespace bluetoe {
namespace link_layer {

    class delta_time
    {
    public:
        explicit delta_time( std::uint32_t usec );
        static delta_time usec( std::uint32_t usec );
        static delta_time now();

    private:
        std::uint32_t usec_;
    };

}
}
#endif
