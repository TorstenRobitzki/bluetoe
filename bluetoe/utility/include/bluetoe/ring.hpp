#ifndef BLUETOE_UTILITY_RING_HPP
#define BLUETOE_UTILITY_RING_HPP

#include <atomic>
#include <cstdint>

namespace bluetoe {
namespace details {

    /**
     * @brief an atomic ring buffer
     *
     * Ring buffer that supports a single consumer, single producer which
     * do not have to run in the same CPU context.
     */
    template < std::size_t S, typename T >
    class ring
    {
    public:
        /**
         * @brief contructs an empty ring
         */
        ring();

        bool try_push( const T& );

        bool try_pop( T& );

    private:
        // queue is empty, if both point to the very same element
        // if read_ptr_ != write_ptr_, the ring is not empty and data_[ read_ptr_ ]
        // contains the next element to read from.
        std::atomic_int read_ptr_;
        std::atomic_int write_ptr_;

        static constexpr std::size_t length = S + 1;

        T data_[ length ];
    };

    // implementation
    template < std::size_t S, typename T >
    ring< S, T >::ring()
        : read_ptr_( 0 )
        , write_ptr_( 0 )
    {
        static_assert( ATOMIC_INT_LOCK_FREE, "atomic_int is expected to be lock free" );
    }

    template < std::size_t S, typename T >
    bool ring< S, T >::try_push( const T& in )
    {
        const int read  = read_ptr_.load();
        const int write = write_ptr_.load();
        const int next  = ( write + 1 ) % length;

        if ( next == read )
            return false;

        data_[ write ] = in;
        write_ptr_.store( next );

        return true;
    }

    template < std::size_t S, typename T >
    bool ring< S, T >::try_pop( T& out )
    {
        const int read  = read_ptr_.load();
        const int write = write_ptr_.load();

        if ( read == write )
            return false;

        const int next  = ( read + 1 ) % length;

        out = data_[ read ];
        read_ptr_.store( next );

        return true;
    }

}
}

#endif
