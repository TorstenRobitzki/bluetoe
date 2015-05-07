#ifndef BLUETOE_WRITE_QUEUE_HPP
#define BLUETOE_WRITE_QUEUE_HPP

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <utility>

namespace bluetoe {

    namespace details {
        struct write_queue_meta_type;
    }

    /**
     * @brief defines a write queue size that is shared among all connected clients
     *
     * Defines the size of a per server write queue in bytes. The queue is allocated within the server object.
     * All connected clients share the same write queue. The write queue is needed to implement the
     * "Write Long Characteristic" procedure defined by GATT.
     *
     * To write objects of the size N, the queue size must be ( ( N + 17 ) / 18 * 7 ) + N bytes (integer math). For example, if the
     * size of the largest object to be writen is 100 bytes, than the queue size must be ( ( 100 + 17 ) / 18 * 7 ) + 100 = 142
     *
     * As all connections share one write queue, the whole queue is allocated to the first connection that starts writing a long characteristic
     * value until that client is done with writing or gets disconnected. All othere connections will get an "Prepare Queue Full" error meanwhile.
     *
     * If no write queue size if given, the server will respond with "Request Not Supported" to a ATT "Prepare Write Request" or "Execute Write Request".
     *
     * @sa server
     *
     * example:
     * @code
    typedef bluetoe::server<
        bluetoe::shared_write_queue< 128 >,
    ...
    > large_object_server;

    typedef bluetoe::extend_server<
        small_temperature_service,
        bluetoe::server_name< name >
    > small_named_temperature_service;

     * @endcode
     */
    template < std::uint16_t S >
    struct shared_write_queue {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::write_queue_meta_type meta_type;

        static constexpr std::uint16_t queue_size = S;
        /** @endcond */
    };

namespace details {

    /*
     * Interface to access a write queue. All member named are some kind of prefixed with 'write_queue', because this type will be fixed into the server
     */
    template < typename QueueParameter >
    class write_queue;

    template < std::uint16_t S >
    class write_queue< shared_write_queue< S > >
    {
    public:
        write_queue();

        /*
         * Allocate n bytes for the given client. Returns 0 if it was not possible to allocate out of the queue.
         * If successful, the function returns the start of an n byte size array.
         */
        template < typename ConData >
        std::uint8_t* allocate_from_write_queue( std::size_t n, ConData& client );

        /*
         * Deallocates all elements allocated by the given client
         */
        template < typename ConData >
        void free_write_queue( ConData& client );

        /*
         * return the first element of the queue for the given client. If no element was allocated, the function returns nullptr.
         */
        template < typename ConData >
        std::pair< std::uint8_t*, std::size_t > first_write_queue_element( ConData& client );

        /*
         * Returns the next element in the queue to current. current must not be nullptr and must be obtained by a call to
         * first_element() or next_element()
         */
        template < typename ConData >
        std::pair< std::uint8_t*, std::size_t > next_write_queue_element( std::uint8_t* current, ConData& );
    private:

        std::size_t read_size( std::uint8_t* ) const;

        void*           current_client_;
        std::uint8_t    buffer_[ S ];
        std::uint16_t   buffer_end_;
    };

    struct no_such_type;

    template <>
    class write_queue< no_such_type >
    {
    public:
        template < typename ConData >
        void free_write_queue( ConData& ) {}
    };

    /*
     * guard to make sure, that free_write_queue is called when this guard goes out of scope
     */
    template < typename ConData, typename WriteQueue >
    class write_queue_guard
    {
    public:
        write_queue_guard( ConData& client, WriteQueue& queue );
        ~write_queue_guard();

    private:
        write_queue_guard( const write_queue_guard& ) = delete;
        write_queue_guard& operator=( const write_queue_guard& ) = delete;

        ConData&    client_;
        WriteQueue& queue_;
    };

    // implementation
    template < std::uint16_t S >
    write_queue< shared_write_queue< S > >::write_queue()
        : current_client_( nullptr )
        , buffer_end_( 0 )
    {
    }

    template < std::uint16_t S >
    template < typename ConData >
    std::uint8_t* write_queue< shared_write_queue< S > >::allocate_from_write_queue( std::size_t size, ConData& client )
    {
        assert( size );

        if ( size + 2 > S - buffer_end_ || current_client_ != nullptr && current_client_ != &client )
            return nullptr;

        buffer_[ buffer_end_ ]     = size & 0xff;
        buffer_[ buffer_end_ + 1 ] = size >> 8;

        current_client_ = &client;
        buffer_end_    += size + 2;

        return &buffer_[ buffer_end_ - size ];
    }

    template < std::uint16_t S >
    template < typename ConData >
    void write_queue< shared_write_queue< S > >::free_write_queue( ConData& client )
    {
        if( current_client_ == &client )
        {
            buffer_end_     = 0;
            current_client_ = nullptr;
        }
    }

    template < std::uint16_t S >
    template < typename ConData >
    std::pair< std::uint8_t*, std::size_t > write_queue< shared_write_queue< S > >::first_write_queue_element( ConData& client )
    {
        if( current_client_ != &client || buffer_end_ == 0 )
        {
            return std::make_pair( nullptr, 0 );
        }

        return std::make_pair( &buffer_[ 2 ], read_size( &buffer_[ 2 ] ) );
    }

    template < std::uint16_t S >
    template < typename ConData >
    std::pair< std::uint8_t*, std::size_t > write_queue< shared_write_queue< S > >::next_write_queue_element( std::uint8_t* last, ConData& client )
    {
        assert( last );
        assert( last >= &buffer_[ 2 ] );
        assert( last <= &buffer_[ S ] );
        assert( &client == current_client_ );

        const std::size_t size = read_size( last );

        // lets point last to the next size value
        last += size;

        return last == &buffer_[ buffer_end_ ]
            ? std::make_pair( nullptr, 0 )
            : std::make_pair( last + 2, read_size( last + 2 ) );
    }

    template < std::uint16_t S >
    std::size_t write_queue< shared_write_queue< S > >::read_size( std::uint8_t* last ) const
    {
        return *( last - 2 ) + *( last - 1 ) * 256;
    }

    template < typename ConData, typename WriteQueue >
    write_queue_guard< ConData, WriteQueue >::write_queue_guard( ConData& client, WriteQueue& queue )
        : client_( client )
        , queue_( queue )
    {
    }

    template < typename ConData, typename WriteQueue >
    write_queue_guard< ConData, WriteQueue >::~write_queue_guard()
    {
        queue_.free_write_queue( client_ );
    }

}
}
#endif