#ifndef BLUETOE_SERVICES_BOOTLOADER_HPP
#define BLUETOE_SERVICES_BOOTLOADER_HPP

#include <bluetoe/service.hpp>

namespace bluetoe
{
    namespace bootloader {
        namespace details {
            struct handler_meta_type {};
            struct memory_region_meta_type {};
        };

        template < std::size_t PageSize >
        struct page_size {};

        template < std::size_t PageAlign >
        struct page_align {};

        template < std::uintptr_t Start, std::uintptr_t End >
        struct memory_region {};

        template < typename ... Regions >
        struct white_list;

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

        template < typename UserHandler >
        struct handler {
            typedef UserHandler                 user_handler;
            typedef details::handler_meta_type  meta_type;
        };

        using service_uuid       = bluetoe::service_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8A9 >;
        using control_point_uuid = bluetoe::characteristic_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8A9 >;
        using data_uuid          = bluetoe::characteristic_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8AA >;

        enum att_error_codes : std::uint8_t {
            no_operation_in_progress = bluetoe::error_codes::application_error_start,
            invalid_opcode,
            start_address_invalid,
            start_address_not_accaptable
        };

        enum error_codes : std::uint8_t {};

        namespace details {

            enum opcode : std::uint8_t
            {
                get_version,
                flash_address,
                undefined_opcode = 0xff
            };

            enum response_code : std::uint8_t
            {
                version_response = 1,
                address_accepted
            };

            template < typename UserHandler, typename MemRegions >
            struct handler : UserHandler
            {
                handler()
                    : opcode( undefined_opcode )
                    , start_address( 0 )
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

                            if ( !MemRegions::acceptable( start_address ) )
                            {
                                opcode = undefined_opcode;
                                return std::pair< std::uint8_t, bool >{ start_address_not_accaptable, false };
                            }

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

                std::uint8_t bootloader_write_data( std::size_t write_size, const std::uint8_t* value )
                {
                    return no_operation_in_progress;
                }

                std::uint8_t bootloader_read_data( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    return 0;
                }

                std::uint8_t    opcode;
                std::uintptr_t  start_address;
            };

            template < typename ... Options >
            struct calculate_service {
                using mem_regions  = typename bluetoe::details::find_by_meta_type< memory_region_meta_type, Options... >::type;

                static_assert( !std::is_same< bluetoe::details::no_such_type, mem_regions >::value,
                    "Please use white_list or black_list, to define accessable memory regions." );

                using user_handler = typename bluetoe::details::find_by_meta_type< handler_meta_type, Options... >::type;

                static_assert( !std::is_same< bluetoe::details::no_such_type, user_handler >::value,
                    "To use the bootloader, please provide a handler<> that fullfiles the requirements documented with bootloader_handler_prototype." );

                using implementation = handler< typename user_handler::user_handler, mem_regions >;

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
                        bluetoe::mixin_write_handler<
                            implementation, &implementation::bootloader_write_data
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

    }

    template < typename ... Options >
    using bootloader_service = typename bootloader::details::calculate_service< Options... >::type;

    /**
     * @brief Prototype for a handler, that adapts the bootloader service to the actual hardware
     */
    class bootloader_handler_prototype
    {
    public:
        /*
         * Start to flash
         */
        bootloader::error_codes start_flash( const std::uint8_t* target, std::uint8_t* destination, std::size_t size );
        bootloader::error_codes start_copy( const std::uint8_t* target, std::uint8_t* destination, std::size_t size );
        std::uint32_t start_checksum( const std::uint8_t* target, std::uint8_t* destination, std::size_t size );

        bootloader::error_codes run( const std::uint8_t* start_addr );

        std::pair< const std::uint8_t*, std::size_t > get_version();
    };
}

#endif