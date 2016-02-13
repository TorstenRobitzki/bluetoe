#ifndef BLUETOE_SERVICES_BOOTLOADER_HPP
#define BLUETOE_SERVICES_BOOTLOADER_HPP

#include <bluetoe/service.hpp>
#include <algorithm>

/**
 * @file services/bootloader.hpp
 *
 * This is the implementation of a bootloader as a Bluetooth LE service.
 * The service contains two characteristics. A control point to inquire informations and to control the flash process.
 * One data characteristic to upload and download larger amounts of data.
 *
 * @todo document the protocol here...
 */
namespace bluetoe
{
    namespace bootloader {

        /** @cond HIDDEN_SYMBOLS */
        namespace details {
            struct handler_meta_type {};
            struct memory_region_meta_type {};
            struct page_size_meta_type {};
        };
        /** @endcond */

        /**
         * @brief required parameter to define the size of a page
         */
        template < std::size_t PageSize >
        struct page_size
        {
            typedef details::page_size_meta_type meta_type;
            static constexpr std::size_t value = PageSize;
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
            static bool acceptable( std::uintptr_t addr )
            {
                return Start <= addr && addr < End
                    || white_list< Regions... >::acceptable( addr );
            }

            typedef details::memory_region_meta_type meta_type;
        };

        template <>
        struct white_list<>
        {
            static bool acceptable( std::uintptr_t )
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
            typedef UserHandler                 user_handler;
            typedef details::handler_meta_type  meta_type;
        };

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

        enum att_error_codes : std::uint8_t {
            no_operation_in_progress = bluetoe::error_codes::application_error_start,
            invalid_opcode,
            start_address_invalid,
            start_address_not_accaptable
        };

        enum class error_codes : std::uint8_t {
            success
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
                    return PageSize - ptr_;
                }

                template < class Handler >
                void set_start_address( std::uintptr_t address, Handler& h )
                {
                    assert( state_ == idle );
                    state_ = filling;
                    ptr_   = address % PageSize;
                    addr_  = address - ptr_;

                    h.read_mem( addr_, ptr_, &buffer_[ 0 ] );
                }

                template < class Handler >
                std::size_t write_data( std::size_t write_size, const std::uint8_t* value, Handler& h )
                {
                    assert( state_ == filling );
                    assert( ptr_ != PageSize );

                    const std::size_t copy_size = std::min( PageSize - ptr_, write_size );
                    std::copy( value, value + copy_size, &buffer_[ ptr_ ] );

                    ptr_ += copy_size;

                    if ( ptr_ == PageSize )
                        flush( h );

                    return free_size();
                }

                template < class Handler >
                void flush( Handler& h )
                {
                    assert( state_ == filling );
                    state_ = flashing;

                    if ( ptr_ != PageSize )
                        h.read_mem( addr_ + ptr_, PageSize - ptr_, &buffer_[ ptr_ ] );

                    h.start_flash( addr_, &buffer_[ 0 ], PageSize );
                }
            private:
                enum {
                    idle,
                    filling,
                    flashing
                } state_;

                std::uintptr_t  addr_;
                std::size_t     ptr_;
                std::uint8_t    buffer_[ PageSize ];
            };

            enum opcode : std::uint8_t
            {
                get_version,
                flash_address,
                flash_flush,
                undefined_opcode = 0xff
            };

            enum response_code : std::uint8_t
            {
                version_response = 1,
                address_accepted
            };

            enum data_code : std::uint8_t
            {
                success         = 1,
                /** a complete page was received, no flush necessary */
                success_page
            };

            template < typename UserHandler, typename MemRegions, std::size_t PageSize >
            class handler : public UserHandler
            {
            public:
                handler()
                    : opcode( undefined_opcode )
                    , start_address( 0 )
                    , check_sum( 0 )
                    , next_buffer_( 0 )
                {
                }

                std::pair< std::uint8_t, bool > bootloader_write_control_point( std::size_t write_size, const std::uint8_t* value )
                {
                    if ( write_size < 1 )
                        return std::make_pair( bluetoe::error_codes::invalid_attribute_value_length, false );

                    opcode = *value;

                    switch ( opcode )
                    {
                    case get_version:
                        return std::pair< std::uint8_t, bool >{ bluetoe::error_codes::success, true };
                    case flash_address:
                        {
                            if ( write_size != 1 + sizeof( std::uint8_t* ) )
                            {
                                opcode = undefined_opcode;
                                return std::pair< std::uint8_t, bool >{ start_address_invalid, false };
                            }

                            start_address = 0;
                            for ( const std::uint8_t* rbegin = value + write_size, *rend = value +1; rbegin != rend; --rbegin )
                            {
                                start_address = start_address << 8;
                                start_address |= *( rbegin -1 );
                            }

                            check_sum = this->checksum32( value +1, write_size -1, 0 );

                            if ( !MemRegions::acceptable( start_address ) )
                            {
                                opcode = undefined_opcode;
                                return std::pair< std::uint8_t, bool >{ start_address_not_accaptable, false };
                            }

                            buffers_[next_buffer_].set_start_address( start_address, *this );

                            return std::pair< std::uint8_t, bool >{ bluetoe::error_codes::success, false };
                        }
                    case flash_flush:
                        {
                            buffers_[next_buffer_].flush( *this );
                            return std::pair< std::uint8_t, bool >{ bluetoe::error_codes::success, false };
                        }
                    }

                    return std::pair< std::uint8_t, bool >{ att_error_codes::invalid_opcode, false };
                }

                std::uint8_t bootloader_read_control_point( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    switch ( opcode )
                    {
                    case get_version:
                        {
                            const std::pair< const std::uint8_t*, std::size_t > version = this->get_version();
                            out_size = std::min( out_size, version.second + 1 );

                            assert( out_size > 0 );
                            *out_buffer = version_response;
                            std::copy( version.first, version.first + out_size - 1, out_buffer + 1 );
                        }
                        break;
                    }

                    return bluetoe::error_codes::success;
                }

                std::pair< std::uint8_t, bool > bootloader_write_data( std::size_t write_size, const std::uint8_t* value )
                {
                    if ( opcode == undefined_opcode )
                        return std::pair< std::uint8_t, bool >( no_operation_in_progress, false );

                    check_sum = this->checksum32( value, write_size, check_sum );

                    buffers_[next_buffer_].write_data( write_size, value, *this );

                    return std::pair< std::uint8_t, bool >( bluetoe::error_codes::success, true );
                }

                std::uint8_t bootloader_read_data( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    static constexpr std::size_t pdu_size = 10;

                    assert( read_size >= pdu_size );

                    out_buffer[ 0 ] = success;
                    out_buffer[ 1 ] = 23; /// @todo how to get the current MTU size here?
                    out_buffer += 2;

                    out_buffer = bluetoe::details::write_32bit( out_buffer, free_size() );
                    out_buffer = bluetoe::details::write_32bit( out_buffer, check_sum );

                    out_size = pdu_size;

                    return bluetoe::error_codes::success;
                }

            private:
                std::uint32_t free_size() const
                {
                    std::uint32_t result = 0;

                    for ( const auto& b : buffers_ )
                        result += b.free_size();

                    return result;
                }

                std::uint8_t                    opcode;
                std::uintptr_t                  start_address;
                std::uint32_t                   check_sum;

                static constexpr std::size_t    number_of_concurrent_flashs = 2;
                unsigned                        next_buffer_;
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

                using implementation = handler< typename user_handler::user_handler, mem_regions, page_size::value >;

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
                        bluetoe::notify
                    >,
                    bluetoe::characteristic<
                        bluetoe::bootloader::data_uuid,
                        bluetoe::mixin_write_notification_control_point_handler<
                            implementation, &implementation::bootloader_write_data, bluetoe::bootloader::data_uuid
                        >,
                        bluetoe::mixin_read_handler<
                            implementation, &implementation::bootloader_read_data
                        >,
                        bluetoe::no_read_access,
                        bluetoe::notify
                    >,
                    bluetoe::mixin< implementation >
                >;
            };
        }
        /** @endcond */

        /**
         * @brief Prototype for a handler, that adapts the bootloader service to the actual hardware
         */
        class bootloader_handler_prototype
        {
        public:
            /**
             * Start to flash
             */
            bootloader::error_codes start_flash( std::uintptr_t address, const std::uint8_t* values, std::size_t size );

            bootloader::error_codes run( std::uintptr_t start_addr );

            std::pair< const std::uint8_t*, std::size_t > get_version();

            /*
             * this is very handy, when it comes to testing...
             */
            void read_mem( std::uintptr_t address, std::size_t size, std::uint8_t* destination );

            std::uint32_t checksum32( const std::uint8_t* values, std::size_t size, std::uint32_t init );
        };
    }

    /**
     * @brief Implementation of a bootloader service
     */
    template < typename ... Options >
    using bootloader_service = typename bootloader::details::calculate_service< Options... >::type;

}

#endif