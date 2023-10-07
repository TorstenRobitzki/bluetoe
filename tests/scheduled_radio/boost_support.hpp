struct get_time_tag_t {};

#define CLOCK_MONOTONIC static_cast< const get_time_tag_t* >( nullptr )

void* clock_gettime( const get_time_tag_t*, void* );