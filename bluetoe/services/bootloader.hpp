#ifndef BLUETOE_SERVICES_BOOTLOADER_HPP
#define BLUETOE_SERVICES_BOOTLOADER_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/mixin.hpp>
#include <algorithm>

/**
 * @file services/bootloader.hpp
 *
 * This is the implementation of a bootloader as a Bluetooth LE service.
 * The service contains two characteristics. A control point to inquire informations and to control the flash process.
 * One data characteristic to upload and download larger amounts of data.
 *
 * \ref Bootloader-Protocol
 */
namespace bluetoe
{
    namespace bootloader {

        /** @cond HIDDEN_SYMBOLS */
        namespace details {
            struct handler_meta_type {};
            struct memory_region_meta_type {};
            struct page_size_meta_type {};
        }
        /** @endcond */

        /**
         * @brief required parameter to define the size of a page
         */
        template < std::size_t PageSize >
        struct page_size
        {
           /** @cond HIDDEN_SYMBOLS */
            typedef details::page_size_meta_type meta_type;
            static constexpr std::size_t value = PageSize;
            /** @endcond */
        };

        /**
         * @brief denoting a memory range with it's start address and endaddress (exklusive)
         *
         * So memory_region< 0x1000, 0x2000 > means all bytes with the address from 0x1000
         * to 0x2000 but _not_ 0x2000.
         */
        template < std::uintptr_t Start, std::uintptr_t End >
        struct memory_region {};

        /**
         * @brief a list of all memory regions that are flashable by the bootloader
         */
        template < typename ... Regions >
        struct white_list;

        /** @cond HIDDEN_SYMBOLS */
        template < std::uintptr_t Start, std::uintptr_t End, typename ... Regions >
        struct white_list< memory_region< Start, End >, Regions ... >
        {
            static bool acceptable( std::uintptr_t start, std::uintptr_t end )
            {
                return ( start >= Start && end <= End )
                    || white_list< Regions... >::acceptable( start, end );
            }

            typedef details::memory_region_meta_type meta_type;
        };

        template <>
        struct white_list<>
        {
            static bool acceptable( std::uintptr_t, std::uintptr_t )
            {
                return false;
            }

            typedef details::memory_region_meta_type meta_type;
        };
        /** @endcond */

        /**
         * @brief requireded parameter to define, how the bootloader can access memory and flash
         */
        template < typename UserHandler >
        struct handler {
            /** @cond HIDDEN_SYMBOLS */
            typedef UserHandler                 user_handler;
            typedef details::handler_meta_type  meta_type;
            /** @endcond */
        };

        /**
         * @brief inform the bootloader, that an asynchrone flash operations is finished
         */
        template < class Server >
        void end_flash( Server& srv )
        {
            srv.end_flash( srv );
        }

        /**
         * @brief the UUID of the bootloader service is 7D295F4D-2850-4F57-B595-837F5753F8A9
         */
        using service_uuid       = bluetoe::service_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8A9 >;

        /**
         * @brief the UUID of the control point charateristic is 7D295F4D-2850-4F57-B595-837F5753F8A9
         */
        using control_point_uuid = bluetoe::characteristic_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8A9 >;

        /**
         * @brief the UUID of the data charateristic is 7D295F4D-2850-4F57-B595-837F5753F8AA
         */
        using data_uuid          = bluetoe::characteristic_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8AA >;

        /**
         * @brief the UUID of the data charateristic is 7D295F4D-2850-4F57-B595-837F5753F8AA
         */
        using progress_uuid      = bluetoe::characteristic_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8AB >;

        /**
         * @brief error codes that are used by the bootloader on the ATT layer
         */
        enum att_error_codes : std::uint8_t {
            /*
             * attempt to write or read data to or from the bootloader, while no operation is in progress
             */
            no_operation_in_progress = bluetoe::error_codes::application_error_start,
            invalid_opcode,
            invalid_state,
            buffer_overrun_attempt
        };

        /**
         * @brief range of error codes that can be used by the user handler to indicate the cause of an error
         */
        enum class error_codes : std::uint8_t {
            /**
             * every thing is fine.
             */
            success,

            /**
             * somehow the request would require more authorization
             */
            not_authorized,
        };

        /** @cond HIDDEN_SYMBOLS */
        namespace details {

            /**
             * Buffer to take at max a page full of data
             * Responsible to make sure, that full pages are flashed.
             */
            template < std::size_t PageSize >
            class flash_buffer
            {
            public:
                flash_buffer()
                    : state_( idle )
                    , ptr_( 0 )
                {
                }

                std::uint32_t free_size() const
                {
                    return state_ == filling
                        ? PageSize - ptr_
                        : 0;
                }

                template < class Handler >
                void set_start_address( std::uintptr_t address, Handler& h, std::uint32_t checksum, std::uint16_t cons )
                {
                    assert( state_ == idle );
                    state_          = filling;
                    ptr_            = address % PageSize;
                    addr_           = address - ptr_;
                    crc_            = checksum;
                    consecutive_    = cons;

                    h.read_mem( addr_, ptr_, &buffer_[ 0 ] );
                }

                template < class Handler >
                std::size_t write_data( std::size_t write_size, const std::uint8_t* value, Handler& h )
                {
                    assert( state_ == filling );
                    assert( ptr_ != PageSize );

                    const std::size_t copy_size = std::min( PageSize - ptr_, write_size );
                    std::copy( value, value + copy_size, &buffer_[ ptr_ ] );
                    crc_ = h.checksum32( &buffer_[ ptr_ ], copy_size, crc_ );

                    ptr_ += copy_size;

                    if ( ptr_ == PageSize )
                    {
                        flush( h );
                    }

                    return copy_size;
                }

                template < class Handler >
                bool flush( Handler& h )
                {
                    if ( state_ != filling || ptr_ == 0 )
                        return false;

                    state_ = flashing;

                    // no unnessary calls to user handler
                    if ( PageSize !=  ptr_ )
                        h.read_mem( addr_ + ptr_, PageSize - ptr_, &buffer_[ ptr_ ] );

                    h.start_flash( addr_, &buffer_[ 0 ], PageSize );

                    return true;
                }

                std::uint32_t crc() const
                {
                    return crc_;
                }

                void free()
                {
                    state_ = idle;
                    ptr_   = 0;
                }

                bool empty() const
                {
                    return state_ == idle;
                }

                std::uint16_t consecutive() const
                {
                    return consecutive_;
                }
            private:
                enum {
                    idle,
                    filling,
                    flashing
                } state_;

                std::uintptr_t  addr_;
                std::size_t     ptr_;
                std::uint32_t   crc_;
                std::uint8_t    buffer_[ PageSize ];
                std::uint16_t   consecutive_;
            };

            enum opcode : std::uint8_t
            {
                opc_get_version,
                opc_get_crc,
                opc_get_sizes,
                opc_start_flash,
                opc_stop_flash,
                opc_flush,
                opc_start,
                opc_reset,
                opc_read,
                undefined_opcode = 0xff
            };

            enum data_code : std::uint8_t
            {
                success         = 1,
            };

            template < typename UserHandler, typename MemRegions, std::size_t PageSize >
            class controller : public UserHandler
            {
            public:
                controller()
                    : opcode( undefined_opcode )
                    , start_address( 0 )
                    , end_address( 0 )
                    , check_sum( 0 )
                    , in_flash_mode( false )
                    , next_buffer_( 0 )
                    , used_buffer_( 0 )
                    , consecutive_( 0 )
                {
                }

                std::uintptr_t read_address( const std::uint8_t* rend )
                {
                    std::uintptr_t result = 0;
                    for ( const std::uint8_t* rbegin = rend + sizeof( std::uint8_t* ); rbegin != rend; --rbegin )
                    {
                        result = result << 8;
                        result |= *( rbegin -1 );
                    }

                    return result;
                }

                std::pair< std::uint8_t, bool > bootloader_write_control_point( std::size_t write_size, const std::uint8_t* value )
                {
                    if ( write_size < 1 )
                        return std::make_pair( bluetoe::error_codes::invalid_attribute_value_length, false );

                    opcode = *value;

                    switch ( opcode )
                    {
                    case opc_get_version:
                    case opc_get_sizes:
                    case opc_stop_flash:
                        {
                            if ( write_size != 1 )
                                return request_error( bluetoe::error_codes::invalid_attribute_value_length );

                            in_flash_mode = false;
                            next_buffer_  = 0;
                            used_buffer_  = 0;

                            for ( auto& buffer : buffers_ )
                                buffer.free();
                        }
                        break;
                    case opc_get_crc:
                        {
                            if ( write_size != 1 + 2 * sizeof( std::uint8_t* ) )
                                return request_error( bluetoe::error_codes::invalid_attribute_value_length );

                                                 start_address = read_address( value +1 );
                            const std::uintptr_t end_address   = read_address( value +1 + sizeof( std::uint8_t* ) );

                            if ( start_address > end_address || !MemRegions::acceptable( start_address,end_address ) )
                                return request_error( bluetoe::error_codes::invalid_offset );

                            check_sum = this->public_checksum32( start_address, end_address - start_address );
                        }
                        break;
                    case opc_start_flash:
                        {
                            if ( write_size != 1 + sizeof( std::uint8_t* ) )
                                return request_error( bluetoe::error_codes::invalid_attribute_value_length );

                            start_address = read_address( value +1 );
                            check_sum     = this->checksum32( start_address );
                            consecutive_  = 0;

                            if ( !MemRegions::acceptable( start_address, start_address ) )
                                return request_error( bluetoe::error_codes::invalid_offset );

                            buffers_[next_buffer_].set_start_address( start_address, *this, check_sum, consecutive_ );
                            in_flash_mode = true;
                        }
                        break;
                    case opc_flush:
                        {
                            if ( !in_flash_mode )
                                return request_error( invalid_state );

                            if ( !buffers_[next_buffer_].flush( *this ) )
                                return request_error( invalid_state );

                            return std::pair< std::uint8_t, bool >{ bluetoe::error_codes::success, true };
                        }
                        break;
                    case opc_start:
                        {
                            if ( write_size != 1 + sizeof( std::uint8_t* ) )
                                return request_error( bluetoe::error_codes::invalid_attribute_value_length );

                            const std::uintptr_t start_address = read_address( value +1 );

                            this->run( start_address );
                        }
                        break;
                    case opc_reset:
                        {
                            if ( write_size != 1 )
                                return request_error( bluetoe::error_codes::invalid_attribute_value_length );

                            this->reset();
                        }
                    case opc_read:
                        {
                            error         = error_codes::success;
                            start_address = read_address( value +1 );
                            end_address   = read_address( value +1 + sizeof( std::uint8_t* ) );
                            check_sum     = this->checksum32( start_address );

                            if ( start_address > end_address || !MemRegions::acceptable( start_address,end_address ) )
                                return request_error( bluetoe::error_codes::invalid_offset );

                            if ( start_address != end_address )
                            {
                                this->more_data_call_back();

                                return std::pair< std::uint8_t, bool >{ bluetoe::error_codes::success, false };
                            }
                        }
                    default:
                        return std::pair< std::uint8_t, bool >{ att_error_codes::invalid_opcode, false };
                    }

                    return std::pair< std::uint8_t, bool >{ bluetoe::error_codes::success, true };
                }

                std::uint8_t bootloader_read_control_point( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    assert( read_size >= 20 );
                    *out_buffer = opcode;

                    switch ( opcode )
                    {
                    case opc_get_version:
                        {
                            const std::pair< const std::uint8_t*, std::size_t > version = this->get_version();
                            out_size = std::min( read_size, version.second + 1 );

                            *out_buffer = opc_get_version;
                            std::copy( version.first, version.first + out_size - 1, out_buffer + 1 );
                        }
                        break;
                    case opc_get_crc:
                        {
                            bluetoe::details::write_32bit( out_buffer + 1, check_sum );
                            out_size = sizeof( std::uint8_t ) + sizeof( std::uint32_t );
                        }
                        break;
                    case opc_get_sizes:
                        {
                            std::uint8_t* out = out_buffer;
                            ++out;
                            *out = sizeof( std::uint8_t* );
                            ++out;
                            out = bluetoe::details::write_32bit( out, PageSize );
                            out = bluetoe::details::write_32bit( out, number_of_concurrent_flashs );

                            out_size = out - out_buffer;
                        }
                        break;
                    case opc_start_flash:
                        {
                            std::uint8_t* out = out_buffer;
                            ++out;
                            *out = read_size + 3;
                            ++out;
                            out = bluetoe::details::write_32bit( out, buffers_[next_buffer_].crc() );

                            out_size = out - out_buffer;
                        }
                        break;
                    case opc_stop_flash:
                        out_size = 1;
                        break;
                    case opc_flush:
                        {
                            std::uint8_t* out = out_buffer;
                            ++out;
                            out = bluetoe::details::write_32bit( out, buffers_[next_buffer_].crc() );
                            out = bluetoe::details::write_16bit( out, buffers_[next_buffer_].consecutive() );

                            out_size = out - out_buffer;
                        }
                        break;
                    case opc_read:
                        {
                            std::uint8_t* out = out_buffer;
                            ++out;
                            out = bluetoe::details::write_32bit( out, check_sum );
                            *out = static_cast< std::uint8_t >( error );
                            ++out;

                            out_size = out - out_buffer;
                        }
                        break;
                    }

                    return bluetoe::error_codes::success;
                }

                std::uint8_t bootloader_write_data( std::size_t write_size, const std::uint8_t* value )
                {
                    if ( !in_flash_mode )
                        return no_operation_in_progress;

                    if ( write_size == 0 )
                        return bluetoe::error_codes::success;

                    if ( buffers_[ next_buffer_ ].free_size() == 0 && !find_next_buffer( start_address ) )
                        return buffer_overrun_attempt;

                    while ( write_size )
                    {
                        const std::size_t moved = buffers_[ next_buffer_ ].write_data( write_size, value, *this );

                        value           += moved;
                        write_size      -= moved;
                        start_address   += moved;

                        if ( write_size && !find_next_buffer( start_address ) )
                            return buffer_overrun_attempt;
                    }

                    return bluetoe::error_codes::success;
                }

                std::uint8_t bootloader_read_data( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    if ( opcode == opc_read )
                    {
                        out_size  = std::min( read_size, end_address - start_address );

                        error     = this->public_read_mem( start_address, out_size, out_buffer );

                        if ( error == error_codes::success )
                        {
                            check_sum = this->checksum32( out_buffer, out_size, check_sum );
                            start_address += out_size;
                        }
                        else
                        {
                            out_size = 0;
                        }

                        this->more_data_call_back();
                    }

                    return bluetoe::error_codes::success;
                }

                std::uint8_t bootloader_progress_data( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    buffers_[used_buffer_].free();
                    out_size = 7;
                    assert( read_size >= out_size );

                    out_buffer = bluetoe::details::write_32bit( out_buffer, buffers_[used_buffer_].crc() );
                    out_buffer = bluetoe::details::write_16bit( out_buffer, buffers_[used_buffer_].consecutive() );

                    // trick: that's the link layers MTU size:
                    *out_buffer = read_size + 3;
                    ++out_buffer;

                    used_buffer_ = ( used_buffer_ + 1 ) % number_of_concurrent_flashs;

                    return bluetoe::error_codes::success;
                }

                template < class Server >
                void end_flash( Server& server )
                {
                    server.template notify< progress_uuid >();
                }

                template < class Server >
                void request_read_progress( Server& server )
                {
                    if ( start_address == end_address || error != error_codes::success )
                    {
                        server.template notify< control_point_uuid >();
                    }
                    else
                    {
                        server.template indicate< data_uuid >();
                    }
                }

            private:
                std::uint32_t free_size() const
                {
                    std::uint32_t result = 0;

                    for ( const auto& b : buffers_ )
                        result += b.free_size();

                    return result;
                }

                bool find_next_buffer( std::size_t start_address )
                {
                    const auto next = ( next_buffer_ + 1 ) % number_of_concurrent_flashs;

                    if ( buffers_[ next ].empty() )
                    {
                        ++consecutive_;
                        buffers_[ next ].set_start_address( start_address, *this, buffers_[ next_buffer_ ].crc(), consecutive_ );
                        next_buffer_ = next;

                        return true;
                    }

                    return false;
                }

                std::pair< std::uint8_t, bool > request_error( std::uint8_t code )
                {
                    opcode          = undefined_opcode;
                    in_flash_mode   = false;

                    return std::pair< std::uint8_t, bool >{ code, false };
                }

                std::uint8_t                    opcode;
                std::uintptr_t                  start_address;
                std::uintptr_t                  end_address;
                error_codes                     error;
                std::uint32_t                   check_sum;
                bool                            in_flash_mode;

                static constexpr std::size_t    number_of_concurrent_flashs = 2;
                unsigned                        next_buffer_;
                unsigned                        used_buffer_;
                std::uint16_t                   consecutive_;
                flash_buffer< PageSize >        buffers_[number_of_concurrent_flashs];
            };

            template < typename ... Options >
            struct calculate_service {
                using page_size    = typename bluetoe::details::find_by_meta_type< page_size_meta_type, Options... >::type;

                static_assert( !std::is_same< bluetoe::details::no_such_type, page_size >::value,
                    "Please use page_size<> to define the block size of the flash." );

                using mem_regions  = typename bluetoe::details::find_by_meta_type< memory_region_meta_type, Options... >::type;

                static_assert( !std::is_same< bluetoe::details::no_such_type, mem_regions >::value,
                    "Please use white_list or black_list, to define accessable memory regions." );

                using user_handler = typename bluetoe::details::find_by_meta_type< handler_meta_type, Options... >::type;

                static_assert( !std::is_same< bluetoe::details::no_such_type, user_handler >::value,
                    "To use the bootloader, please provide a handler<> that fullfiles the requirements documented with bootloader_handler_prototype." );

                using implementation = controller< typename user_handler::user_handler, mem_regions, page_size::value >;

                using type = bluetoe::service<
                    bluetoe::bootloader::service_uuid,
                    bluetoe::characteristic<
                        bluetoe::bootloader::control_point_uuid,
                        bluetoe::mixin_write_notification_control_point_handler<
                            implementation, &implementation::bootloader_write_control_point, bluetoe::bootloader::control_point_uuid
                        >,
                        bluetoe::mixin_read_handler<
                            implementation, &implementation::bootloader_read_control_point
                        >,
                        bluetoe::no_read_access,
                        bluetoe::notify,
                        bluetoe::write_without_response
                    >,
                    bluetoe::characteristic<
                        bluetoe::bootloader::data_uuid,
                        bluetoe::mixin_write_handler<
                            implementation, &implementation::bootloader_write_data
                        >,
                        bluetoe::mixin_read_handler<
                            implementation, &implementation::bootloader_read_data
                        >,
                        bluetoe::no_read_access,
                        bluetoe::indicate,
                        bluetoe::write_without_response
                    >,
                    bluetoe::characteristic<
                        bluetoe::bootloader::progress_uuid,
                        bluetoe::mixin_read_handler<
                            implementation, &implementation::bootloader_progress_data
                        >,
                        bluetoe::no_read_access,
                        bluetoe::notify
                    >,
                    bluetoe::mixin< implementation >
                >;
            };
        }
        /** @endcond */

        template < typename UserHandler, typename MemRegions, std::size_t PageSize >
        using controller = details::controller< UserHandler, MemRegions, PageSize >;

        /**
         * @brief Prototype for a handler, that adapts the bootloader service to the actual hardware
         */
        class bootloader_handler_prototype
        {
        public:
            /**
             * Start to flash
             *
             * When the hardware signals, that the memory is flashed, end_flash( server) have to be called on the server instance
             */
            bootloader::error_codes start_flash( std::uintptr_t address, const std::uint8_t* values, std::size_t size );

            /**
             * Run the program given at start_addr
             */
            bootloader::error_codes run( std::uintptr_t start_addr );

            /**
             * reset bootloader
             */
            bootloader::error_codes reset();

            /**
             * Return a custom string as response to the Get Version procedure.
             * Make sure, that the response is not longer than 20 bytes, or othere wise it could get truncated on the link layer.
             */
            std::pair< const std::uint8_t*, std::size_t > get_version();

            /**
             * this is very handy, when it comes to testing...
             */
            void read_mem( std::uintptr_t address, std::size_t size, std::uint8_t* destination );

            /**
             * calculate the checksum of the given range of memory.
             */
            std::uint32_t checksum32( std::uintptr_t start_addr, std::size_t size );

            /**
             * adds new data to the given crc
             */
            std::uint32_t checksum32( const std::uint8_t* start_addr, std::size_t size, std::uint32_t old_crc );

            /**
             * special overload to calculate the CRC over a start address
             */
            std::uint32_t checksum32( std::uintptr_t start_addr );

            /**
             * This function is used, when reading from memory, while the Read procedure is executed.
             *
             * @attention Make sure, that this function does not reveal any sensitiv information.
             *            Make sure, that address and size are resonable parameters.
             *
             * @return an value != error_codes::success, if read access to the given range is not possible or not authorized.
             */
            bootloader::error_codes public_read_mem( std::uintptr_t address, std::size_t size, std::uint8_t* destination );

            /**
             * @brief version of checksum function, that will be directly called by the execution of the
             *        Get CRC procedure.
             *
             * @attention If there are ranges that contain sensitiv information, make sure, that size is large enough,
             *            so that one can not get the content of the memory from the resulting CRC.
             *            In case, that such an attempt is detected, return a fixed value (0 for example).
             */
            std::uint32_t public_checksum32( std::uintptr_t start_addr, std::size_t size );

            /**
             * @brief technical required function, that have to call request_read_progress(), with the instance of the server
             */
            void more_data_call_back();

        };
    }

    /**
     * @brief Implementation of a bootloader service
     */
    template < typename ... Options >
    using bootloader_service = typename bootloader::details::calculate_service< Options... >::type;

}

#endif
