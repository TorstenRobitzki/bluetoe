#ifndef BLUETOE_CHARACTERISTIC_HPP
#define BLUETOE_CHARACTERISTIC_HPP


namespace bluetoe {

    template < const char* const >
    class characteristic_name {};

    template < typename ... Options >
    class characteristic {};

    template < typename T, const T* >
    class bind_characteristic_value
    {
    };

    class read_only {};
}

#endif
