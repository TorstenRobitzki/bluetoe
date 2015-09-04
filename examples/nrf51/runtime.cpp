extern "C" void __cxa_pure_virtual(void) {}

extern "C" void (*__preinit_array_start []) (void) __attribute__((weak));
extern "C" void (*__preinit_array_end []) (void) __attribute__((weak));
extern "C" void (*__init_array_start []) (void) __attribute__((weak));
extern "C" void (*__init_array_end []) (void) __attribute__((weak));
extern "C" void (*__fini_array_start []) (void) __attribute__((weak));
extern "C" void (*__fini_array_end []) (void) __attribute__((weak));

extern "C" typedef void (*vector)();

extern "C" void _start(void) {
    extern int main(void);


    for ( vector* init = &__preinit_array_start[ 0 ]; init != &__preinit_array_end[ 0 ]; ++init )
        (*init)();

    for ( vector* init = &__init_array_start[ 0 ]; init != &__init_array_end[ 0 ]; ++init )
        (*init)();

    main();

    for ( vector* finit = &__fini_array_start[ 0 ]; finit != &__fini_array_end[ 0 ]; ++finit )
        (*finit)();

}